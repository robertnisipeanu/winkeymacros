# winkeymacros
WinKeyMacros is supposed to allow you to use multiple keyboards as individual ones on Windows. You can set the keys of a second/third/[...] keyboard as macro keys (eg. key 'k' on second keyboard, would execute sequence 'hello world').
Currently, the driver is almost finished and can be found inside `KeyboardFilterDriverKMDF` directory. For easier communication with the driver, there is a library that can be used to communicate with it, found inside `MacroLibrary` directory. An example app implementing `MacroLibrary` can be found in `ConsoleCommunicationTest` directory.

Current project goals:
- Build an UI app to manage macros
- Find a signer for the driver (Windows requires signing drivers with an EV certificate, which can only bought by a business and costs a lot of money)
