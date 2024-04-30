# Grove Vision V2 and SenseCAP indicator

This is a simple demo to show how to use the Grove Vision V2 and SenseCAP indicator. And this repo is contains the code of the SenseCAP indicator(ESP32 and RP2040) 

For Grove vision v2, navigate to the wiki page : 🤓[Using Grove Vision AI V2 with AT Commands](https://wiki.seeedstudio.com/grove_vision_ai_v2_at/)

<div style="text-align: center;">
<img src="https://github.com/Seeed-Projects/visionv2-indicator/assets/45476879/060bad95-ac13-46fd-a65f-c26e01e42b1d" width="480" height="auto">
</div>

The SenseCAP indicator get the result data and image from grove vision v2 via `UART` to draw the point, line and display the image.


https://github.com/Seeed-Projects/visionv2-indicator/assets/45476879/152d6720-306d-4f59-886c-b976ab14111a


Let's see the diagram in below:

```mermaid
sequenceDiagram
    indicator->>+grove vision v2: The result of current your see
    grove vision v2->>+indicator: There you go(inference result)
    indicator->>+grove vision v2: Capture a image for me
    grove vision v2->>+indicator: There you go(JPEG{based64})
```

## Prerequisites

- Get and install ESP-IDF toolchain and its dependencies.
  [ESP-IDF Get started](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)

  - Install ESP-IDF on Windows: [Description page](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html)
  - There's a ESP-related IDE too, made by Espressif, containing ESP-IDF, called Espressif-IDE which is based on Eclipse CDT. [Espressif-IDE](https://github.com/espressif/idf-eclipse-plugin/blob/master/docs/Espressif-IDE.md)
    - Get the [ESP-IDF offline Windows installer](https://dl.espressif.com/dl/idf-installer/esp-idf-tools-setup-offline-5.1.1.exe?) or [Espressif-IDE Windows installer](https://dl.espressif.com/dl/idf-installer/espressif-ide-setup-2.11.0-with-esp-idf-5.1.1.exe)
    - Install it on your Windows system accepting all offered options and default settings. This automagically installs Python, git, CMake, etc all at once under C:\Espressif folder.
    - You can start building in command-line from the PowerShell/CMD entries created in the start-menu, but with the help of the included build.bat you can build on a normal commandline too
    - Or you can build the project in the IDE GUI, see 'Usage' section.

More setup details in [ESP-IDF - How To Flash The Native Firmware](https://wiki.seeedstudio.com/SenseCAP_Indicator_How_To_Flash_The_Default_Firmware/)