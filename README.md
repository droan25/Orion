# Orion: Local AI Assistant in C++ with Llama.cpp and Voice I/O

## Features
- Text and voice command interface (macOS)
- LLM integration (Llama.cpp, all unknowns go to Llama)
- System control (open apps, open files, open folders)
- File search functionality
- Volume control (mute/unmute, volume up/down)
- Reminders with SQLite persistence
- Weather information (OpenWeatherMap API)
- YouTube video playback
- Website opening and web search
- Play music plugin (play music <file>)
- Voice output (macOS 'say') with speech interruption
- Command history logging with SQLite
- Configuration storage
- Plugin system for extensibility

## Getting Started

### Prerequisites
- macOS
- clang compiler
- SQLite3 development libraries
- libcurl development libraries
- nlohmann/json library
- Llama.cpp built at `/path/to/llama.cpp/main`
- Model at `/path/to/llama.cpp/models/tinyllama/tinyllama-1.1b-chat-v1.0_Q4_K_M.gguf`

### Build
```bash
clang++ -std=c++17 main.cpp CommandProcessor.cpp StorageHelper.cpp -I/opt/homebrew/include -L/opt/homebrew/lib -lsqlite3 -lcurl -o orion
```

### Run
```bash
./orion
```

## Usage
- Choose input mode: (1) Text  (2) Voice
- Example commands:
  - `open app Safari`
  - `open file /path/to/file.txt`
  - `open folder /path/to/Documents`
  - `search for file document.txt`
  - `remind me to buy milk`
  - `list reminders`
  - `play music /path/to/Music/song.mp3`
  - `weather in London`
  - `play despacito` (YouTube)
  - `open website google.com`
  - `mute` / `unmute` / `volume up` / `volume down`
  - Any other question or command (will go to Llama LLM)
- Press Enter during speech to interrupt
- Type or say `exit` to quit

## Data Storage
- SQLite database (`orion.db`) stores:
  - Command history (user input and Orion responses)
  - Reminders
  - Configuration settings
- History can be cleared with `clear history`

## Extending
- Add more commands in `CommandProcessor.cpp`
- Implement custom plugins by inheriting from the `Plugin` class and registering them with `addPlugin()`
- Add new API integrations following the weather/YouTube examples

## Dependencies
- **SQLite3**: For persistent storage
- **libcurl**: For HTTP requests (weather, YouTube)
- **nlohmann/json**: For JSON parsing
- **Llama.cpp**: For LLM integration

## API Keys

This project requires API keys for YouTube Data API and OpenWeatherMap.
Please replace the placeholder text (e.g., `YOUR_YOUTUBE_API_KEY_HERE`, `YOUR_OPENWEATHERMAP_API_KEY_HERE`) in the code with your own API keys before running.

---
This is a local, private, extensible AI assistant for your Mac! 