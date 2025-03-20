# Browser-Based Emulators

This is a hobby project to develop browser-based emulators for classic gaming machines from the 1970s, 1980s, and 1990s. The project spans from the simple Chip-8 — a rite of passage for any emulator builder — to eventually supporting more advanced 16-bit systems such as the Super Nintendo and Sega Mega Drive (Genesis). The goal is to build these emulators using C++ (compiled to WebAssembly via Emscripten) for the core emulation and TypeScript/WebGL for rendering in the browser.

## Supported Emulators

**Chip-8:**  
Currently, the only supported system is Chip-8, which is in active development. Chip-8 is a simple, interpreted programming language originally developed in the 1970s for home computers. It was designed to simplify game development and is widely considered a great starting point for anyone interested in writing an emulator. Despite its simplicity, Chip-8 provided the foundation for early gaming experiences and remains a popular choice among hobbyist emulator developers.

*Fun fact:* Many classic Chip-8 programs were built for early computers and calculators, and the system has become a "rite of passage" for emulator builders due to its straightforward design and limited instruction set.

## Project Overview

- **Emulator Code (WASM/C++):**  
  The core emulator logic is written in C++ and compiled to WebAssembly (WASM) using Emscripten. This ensures near-native performance within the browser.

- **Rendering (TypeScript/WebGL):**  
  The emulator’s display is rendered using TypeScript and WebGL, providing fast and efficient graphics directly in the browser.

- **Future Plans:**  
  The project is designed to expand beyond Chip-8 to eventually include more complex systems like the Super Nintendo and Sega Mega Drive, each with its unique challenges and hardware quirks.

## Installation & Build Instructions

### Prerequisites

**Emscripten**

Emscripten is required to compile your C++ emulator code to WebAssembly. Follow these steps to install and set it up:

1. Clone the Emscripten SDK repository:
   
   git clone https://github.com/emscripten-core/emsdk.git  
   cd emsdk

2. Install and activate the latest Emscripten version:
   
   ./emsdk install latest  
   ./emsdk activate latest

3. Set up your environment (on Unix-like systems):
   
   source ./emsdk_env.sh

This sets the necessary environment variables so that tools like em++ are available.

**Vite**

This project uses Vite as the development server and build tool for the TypeScript/WebGL application.

1. In the project root (where your package.json is located), install dependencies:
   
   yarn install  
   (or use npm install)

2. The Vite development server will serve your application, which loads chip8.js (the Emscripten glue code) and your main TypeScript module.

### Build Instructions

1. **Compile the Emulator Code:**  
   With Emscripten installed and configured, compile your C++ source (located in wasm/chip8.cpp) using the build script defined in your package.json:
   
   yarn build:wasm

   This produces the chip8.js and chip8.wasm files in your designated output folder (e.g., dist or public).

2. **Run the Development Server:**  
   Start the Vite development server with:
   
   yarn dev  
   (or npm run dev)

   The server will typically serve your application at http://localhost:3000. Ensure your index.html (located in the public folder) loads chip8.js before your main TypeScript module.

## Project Structure

- **public/**
  - `chip8.js` — Emscripten glue code
  - `chip8.wasm` — Compiled WebAssembly module
  - `index.html` — HTML file that loads chip8.js and main.ts
- **src/**
  - `main.ts` — Main TypeScript entry point for WebGL rendering and emulator integration
- **wasm/**
  - `chip8.cpp` — C++ source code for the emulator
- `package.json` — NPM/Yarn configuration and scripts
- `README.md` — This file 


## License

This project is provided as-is under the MIT License.