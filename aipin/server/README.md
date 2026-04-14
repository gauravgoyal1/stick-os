# AiPin WiFi Server

TCP server that receives audio streams from AiPin over WiFi and generates transcripts with speaker identification.

## Setup

```bash
cd server
pip install requests
```

Set your Gemini API key in `.env` (in project root):
```
GEMINI_API_KEY=your_api_key_here
```

## Usage

```bash
# Start the server
python server.py

# With options
python server.py --port 8765 --verbose

# Skip transcription (just save audio)
python server.py --no-transcribe
```

## Configuration

Update `aipin_wifi.ino` with your server's IP address:
```cpp
const char* serverHost = "192.168.1.100";  // Your computer's IP
const uint16_t serverPort = 8765;
```

Also update the WiFi credentials in `aipin_wifi.ino`:
```cpp
WiFiCredentials wifiNetworks[] = {
    {"Your_WiFi_Name", "your_password"},
    // Add more networks...
};
```

## Features

- **Multi-client support**: Handles multiple AiPin devices simultaneously
- **Speaker diarization**: Identifies different speakers (Person 1, Person 2, etc.)
- **Name recognition**: Uses actual names when speakers introduce themselves
- **Auto-save**: Recordings saved to `recordings/` as WAV files
- **Transcripts**: Saved to `transcripts/` as text files

## Output Format

Transcripts are formatted as:
```
Person 1: Hello, my name is John.
John: I work in engineering.
Person 2: Nice to meet you, John. I'm Sarah.
Sarah: I'm from the marketing team.
```
