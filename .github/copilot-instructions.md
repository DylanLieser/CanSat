# CanSat Project - AI Assistant Instructions

This is a PlatformIO-based project for an ESP32-powered CanSat (Can-sized Satellite). The project uses the Arduino framework on ESP32 hardware.

## Project Structure

- `src/` - Main application source files
- `include/` - Header files
- `lib/` - Project-specific libraries
- `test/` - Test files
- `platformio.ini` - PlatformIO project configuration

## Development Environment

- Platform: Espressif32
- Board: ESP32 Dev Module
- Framework: Arduino

## Key Workflows

### Building
```bash
pio run
```

### Uploading
```bash
pio run --target upload
```

### Monitoring Serial Output
```bash
pio device monitor
```

### Running Tests
```bash
pio test
```

## Project Conventions

1. **Code Organization**
   - Main application code goes in `src/`
   - Reusable components belong in `lib/`
   - Headers with public interfaces in `include/`

2. **Configuration**
   - Hardware-specific settings are managed in `platformio.ini`
   - Use environment-based configuration for different build targets

## Dependencies

The project uses the ESP32 Arduino core framework. Additional libraries should be declared in `platformio.ini` using the `lib_deps` directive.

## Getting Started

1. Install PlatformIO Core or PlatformIO IDE
2. Clone the repository
3. Build and upload using PlatformIO commands

## Note for AI Assistants

- This is an embedded systems project - consider memory constraints and hardware limitations
- Use Arduino-style coding patterns when suggesting implementations
- Remember ESP32's dual-core architecture when dealing with concurrent operations
- Consider power efficiency in code suggestions

These instructions will be updated as the project evolves.