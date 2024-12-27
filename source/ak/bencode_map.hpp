#pragma once

#include <unordered_map>
#include <ctype.h>
#include <expected>
#include <cassert>
#include <string>
#include <vector>
#include <span>

class bencode_map {
public:
    enum class target_type {
        unknown = 0,
        integers = 1,
        strings = 2,
        lists = 3,
        dictionaries = 4,
    };

private:
    enum class encoding {
        unknown = '\0',
        integers = 'i',
        lists = 'l',
        dictionaries = 'd',
        ending = 'e',
    };

    enum class states {
        unknown = 0,
        parsing_list = 1,
        parsing_dictionary = 2,
        parsing_word = 3,
        parsing_word_length = 4,
        parsing_integer = 5,
    };

    struct target {
        target_type type { target_type::unknown };
        std::string string_value {};
        int integer_value {};
        std::vector<target> list_container {};
        std::unordered_map<std::string, target> map_container {};
        std::string latest_key {};
        std::vector<std::byte> raw_bencode {};
    };

    states m_previous_state { states::unknown };
    states m_state { states::unknown };
    size_t m_element_length = 0;
    bool m_store_raw_bencode = false;
    std::string m_element {};
    std::string m_element_length_string {};
    std::vector<target*> m_target_queue;
    const target* m_bracket_root {};
    target m_root {};

protected:
    bool can_have_children(const target_type& type)
    {
        return (type == target_type::dictionaries || type == target_type::lists);
    }

    void push_to_target(const target& child_target)
    {
        if (m_target_queue.size() == 0) {
            m_root = child_target;
            if (can_have_children(child_target.type))
                m_target_queue.push_back(&m_root);
            return;
        }

        target* parent_target = m_target_queue.back();

        if (parent_target->type == target_type::lists) {
            parent_target->list_container.push_back(child_target);
            if (can_have_children(child_target.type))
                m_target_queue.push_back(&parent_target->list_container.back());
        }

        if (parent_target->type == target_type::dictionaries) {
            std::string key = parent_target->latest_key;
            if (key.empty()) {
                if (child_target.type != target_type::strings) return;
                parent_target->latest_key = child_target.string_value;
            } else {
                parent_target->map_container[key] = child_target;
                parent_target->latest_key = {};
            }

            if (can_have_children(child_target.type)) {
                m_target_queue.push_back(
                    &parent_target->map_container[key]
                );
            }
        }
    }

    void end_target()
    {
        m_target_queue.pop_back();
    }

    void switch_state(const states& new_state)
    {
        m_previous_state = m_state;
        m_state = new_state;

        if (m_previous_state == states::parsing_word_length) {
            m_element_length = std::stoi(m_element_length_string);
            m_element_length_string = "";
        }

        if (m_previous_state == states::parsing_word) {
            push_to_target({ target_type::strings, m_element });
            m_element = "";
        }

        if (m_previous_state == states::parsing_integer) {
            push_to_target({ target_type::integers, {}, std::stoi(m_element) });
            m_element = "";
        }
    }
    
    void consume_state(const std::byte& c)
    {
        m_state = states::unknown;
        switch ((encoding)c) {
        case encoding::integers:
            m_state = states::parsing_integer;
            break;
        case encoding::lists:
            push_to_target({ target_type::lists });
            m_state = states::parsing_list;
            break;
        case encoding::dictionaries:
            push_to_target({ target_type::dictionaries });
            m_state = states::parsing_dictionary;
            break;
        case encoding::ending:
            end_target();
            break;
        case encoding::unknown:
            m_state = states::unknown;
            break;
        default:
            if (isdigit((int)c)) {
                m_state = states::parsing_word_length;
                m_element_length_string += (char)c;
            }
            break;
        }
    }

    void push_raw_bencode(const std::byte& c)
    {
        for (const auto& target : m_target_queue)
            target->raw_bencode.push_back(c);
    }

    void consume_byte(const std::byte& c)
    {
        if (m_store_raw_bencode)
            push_raw_bencode(c);

        switch (m_state) {
        case states::parsing_integer:
            if (!isdigit((char)c) && (char)c != '-')
                { return switch_state(states::unknown); }
            m_element += (char)c;
            break;

        case states::parsing_word:
            m_element += (char)c;
            m_element_length--;
            if (!m_element_length) { return switch_state(states::unknown); }
            break;

        case states::parsing_word_length:
            if (!isdigit((char)c)) { return switch_state(states::parsing_word); }
            m_element_length_string += (char)c;
            break;

        case states::parsing_list:
            consume_state(c);
            break;

        default:
            consume_state(c);
            break;
        }
    }

public:
    bencode_map() {}
    ~bencode_map() {}

    void store_raw_bencode() { m_store_raw_bencode = true; }

    bencode_map& from_buffer(std::span<std::byte> data)
    {
        for (const auto& e : data) {
            consume_byte(e);
        }
        m_bracket_root = &m_root;
        return *this;
    }

    bencode_map& from_string(const std::string& data)
    {
        for (const auto& e : data) {
            consume_byte((const std::byte)e);
        }
        m_bracket_root = &m_root;
        return *this;
    }

    bencode_map& operator[](const std::size_t& i)
    {
        if (m_bracket_root->type != target_type::lists) return *this;
        m_bracket_root = &m_bracket_root->list_container.at(i);
        return *this;
    }

    bencode_map& operator[](const std::string& i)
    {
        if (m_bracket_root->type != target_type::dictionaries) return *this;
        m_bracket_root = &m_bracket_root->map_container.at(i);
        return *this;
    }

    const target_type type()
    {
        auto type = m_bracket_root->type;
        m_bracket_root = &m_root;
        return type;
    }

    const std::string& as_str()
    {
        assert(m_bracket_root->type == target_type::strings);
        const auto& return_value = m_bracket_root->string_value;
        m_bracket_root = &m_root;
        return return_value;
    }

    const int& as_int()
    {
        assert(m_bracket_root->type == target_type::integers);
        const auto& return_value = m_bracket_root->integer_value;
        m_bracket_root = &m_root;
        return return_value;
    }

    const std::vector<std::byte>& as_raw()
    {
        const auto& return_value = m_bracket_root->raw_bencode;
        m_bracket_root = &m_root;
        return return_value;
    }
};
