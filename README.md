<br />
<div align="center">
  <a href="https://github.com/DevGev/Torr/">
    <img src="https://i.postimg.cc/1XjvPsYS/T.png" alt="Logo" width="190">
  </a>
    <p align="center">
    &nbsp;&nbsp;Torr  /tɔːr/
    <br />
    &nbsp;&nbsp; ᚦᚢᚱᚱ
    <br />
    &nbsp;&nbsp;C++23 bittorrent implementation
    <br />
    <sub>&nbsp;&nbsp;<a href="#roadmap">Roadmap</a>
    ·
    &nbsp;&nbsp;<a href="https://github.com/DevGev/Torr/blob/main/CONTRIBUTE.md">Contribute</a>
    ·
    &nbsp;&nbsp;<a href="https://github.com/DevGev/Torr/archive/refs/heads/main.zip">Download</a></sup>
  </p>
</div>
      
## Getting started

Torr is targeted towards Unix like systems, currently tested on Linux, will be tested on BSDs in the future.
<br />
Will Torr be supported on Windows? Short answer: No.
### Requirements
* openssl
* ninja build
* g++ >= 14.0.0

Build ```libtorr.a``` and copy headers
 ```sh
   ./build.sh release
   ```

### Contribute
See ``CONTRIBUTE.md`` and roadmap below
   
## Roadmap
- [x] magnet uri
- [x] bencode
- [ ] networking
     - [ ] block peers
     - [ ] seeding
     - [ ] dht
     - [ ] utp
     - [ ] proxy
     - [x] udp tracker
     - [x] http tracker
- [ ] storage
    - [ ] caching
    - [ ] local fs
- [ ] sandboxing
    - [ ] sys mseal
    - [x] sys landlock
    - [x] sys seccomp
- [x] concurrency
- [ ] fuzzing
