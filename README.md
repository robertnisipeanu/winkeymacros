# winkeymacros
WinKeyMacros is supposed to allow you to use multiple keyboards as individual ones on Windows. You can set the keys of a second/third/[...] keyboard as macro keys (eg. key 'k' on second keyboard, would execute sequence 'hello world').

WinKeyMacros is composed from the following:
- [KeyboardFilterDriverKMDF](KeyboardFilterDriverKMDF) -> A windows kernel driver, which can intercept and inject keys at kernel-level depending on the source(keyboard) of the key input
- [MacroLibrary](MacroLibrary) -> A library that enables easy communication with the driver (an APP can talk directly to the driver without this, but will have to implement IOCTLs communication by itself, the library takes care of that and offers an easier api)
- The app -> This is not yet ready, there is an example console app (ConsoleCommunicationTest), but the objective is to have a GUI APP which should be easy to use for anyone, even not techy persons.

Currently, the driver is almost finished and can be found inside [KeyboardFilterDriverKMDF](KeyboardFilterDriverKMDF) directory. For easier communication with the driver, there is a library that can be used to communicate with it, found inside [MacroLibrary](MacroLibrary) directory. An example app implementing [MacroLibrary](MacroLibrary) can be found in [ConsoleCommunicationTest](ConsoleCommunicationTest) directory.

For more information of how the driver works, check the README.md from the [driver directory](KeyboardFilterDriverKMDF) and comments from it's code.

Current project goals:
- Build an UI app to manage macros
- Find a signer for the driver (Microsoft requires signing drivers for Win10 with an EV certificate, which can only bought by a business and costs a lot of money and which, unfortunately, could kill this project)
