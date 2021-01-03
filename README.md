# winkeymacros
WinKeyMacros is supposed to allow you to use multiple keyboards as individual ones on Windows. You can set the keys of a second/third/[...] keyboard as macro keys (eg. key 'k' on second keyboard, would execute sequence 'hello world').
Currently, the driver is almost finished and can be found inside [KeyboardFilterDriverKMDF](KeyboardFilterDriverKMDF) directory. For easier communication with the driver, there is a library that can be used to communicate with it, found inside [MacroLibrary](MacroLibrary) directory. An example app implementing [MacroLibrary](MacroLibrary) can be found in [ConsoleCommunicationTest](ConsoleCommunicationTest) directory.

For more information of how the driver works, check the README.md from the [driver directory](KeyboardFilterDriverKMDF) and comments from it's code.

Current project goals:
- Build an UI app to manage macros
- Find a signer for the driver (Microsoft requires signing drivers for Win10 with an EV certificate, which can only bought by a business and costs a lot of money and which, unfortunately, could kill this project)
