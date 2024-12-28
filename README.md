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
    &nbsp;&nbsp;<a href="https://github.com/othneildrew/Best-README-Template/issues/new?labels=bug&template=bug-report---.md">Contribute</a>
    ·
    &nbsp;&nbsp;<a href="https://github.com/othneildrew/Best-README-Template/issues/new?labels=enhancement&template=feature-request---.md">Download</a></sup>
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
   build.sh release
   ```
   
## Roadmap
- [x] magnet uri
- [x] bencode
- [ ] block peers
- [ ] seeding
- [ ] concurrency
- [ ] proxy
- [ ] sandboxing
    - [ ] sys mseal
    - [ ] sys landlock
