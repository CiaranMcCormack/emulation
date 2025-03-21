import Stats from 'stats.js';

// Declare the Module global variable provided by chip8.js
declare var Module: any;

console.log("Hello from main.ts");
console.log("Module global:", Module);

// Create and position the stats panel (for FPS and timing)
const stats = new Stats();
stats.showPanel(0); // 0: FPS
stats.dom.style.left = 'auto';
stats.dom.style.right = '0px';
stats.dom.style.top = '0px';
document.body.appendChild(stats.dom);

// Helper functions (createShader, createProgram) and shader sources
function createShader(gl: WebGLRenderingContext, source: string, type: number): WebGLShader {
  const shader = gl.createShader(type)!;
  gl.shaderSource(shader, source);
  gl.compileShader(shader);
  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
    const err = gl.getShaderInfoLog(shader);
    console.error("Shader compile error:", err);
    gl.deleteShader(shader);
    throw new Error("Shader compile error: " + err);
  }
  return shader;
}

function createProgram(gl: WebGLRenderingContext, vsSource: string, fsSource: string): WebGLProgram {
  const vertexShader = createShader(gl, vsSource, gl.VERTEX_SHADER);
  const fragmentShader = createShader(gl, fsSource, gl.FRAGMENT_SHADER);
  const program = gl.createProgram()!;
  gl.attachShader(program, vertexShader);
  gl.attachShader(program, fragmentShader);
  gl.linkProgram(program);
  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    const err = gl.getProgramInfoLog(program);
    console.error("Program linking error:", err);
    gl.deleteProgram(program);
    throw new Error("Program linking error: " + err);
  }
  return program;
}

const vertexShaderSource = `
  attribute vec2 a_position;
  attribute vec2 a_texCoord;
  varying vec2 v_texCoord;
  void main() {
    gl_Position = vec4(a_position, 0, 1);
    v_texCoord = a_texCoord;
  }
`;

const fragmentShaderSource = `
  precision mediump float;
  varying vec2 v_texCoord;
  uniform sampler2D u_texture;
  void main() {
    float pixel = texture2D(u_texture, v_texCoord).r;
    gl_FragColor = vec4(vec3(pixel), 1.0);
  }
`;

// Load the IBM Logo.ch8 program from the public directory.
async function loadChip8Program(): Promise<Uint8Array> {
  //const response = await fetch('/IBM Logo.ch8');
  const response = await fetch('/pong.ch8');
  const buffer = await response.arrayBuffer();
  return new Uint8Array(buffer);
}

const audioCtx = new AudioContext();
let oscillator: OscillatorNode | null = null;

/**
	•	Checks the Chip‑8 sound timer and starts or stops a beep accordingly.
	•	This function should be called each frame.
*/
function updateSound() {
  // Call the exported getSoundTimer() from the WASM module.
  const st = Module.ccall("getSoundTimer", "number", [], []);
  if (st > 0 && oscillator === null) {
  // Create an oscillator node to play a square wave beep.
  oscillator = audioCtx.createOscillator();
  oscillator.type = "square";
  oscillator.frequency.value = 440; // Frequency in Hz (A4 note)
  oscillator.connect(audioCtx.destination);
  oscillator.start();
  } else if (st === 0 && oscillator !== null) {
  // Stop and disconnect the oscillator if the sound timer reaches 0.
  oscillator.stop();
  oscillator.disconnect();
  oscillator = null;
  }
  }

// Initialize the Chip-8 emulator by loading the program into memory.
async function initEmulator() {
  // Ensure the WASM module is initialized.
  if (!Module.calledRun) {
    await new Promise(resolve => { Module.onRuntimeInitialized = resolve; });
  }
  Module._init();
  const programData = await loadChip8Program();
  // Allocate memory in WASM for the program data.
  const ptr = Module._malloc(programData.length);
  Module.HEAPU8.set(programData, ptr);
  Module.ccall('loadProgram', null, ['number', 'number'], [ptr, programData.length]);
  Module._free(ptr);
}

async function main() {
  await initEmulator();
  const width: number = Module._getScreenWidth();
  const height: number = Module._getScreenHeight();

  const canvas = document.getElementById("glCanvas") as HTMLCanvasElement;
  if (!canvas) {
    console.error("Canvas not found");
    return;
  }
  const gl = canvas.getContext("webgl");
  if (!gl) {
    console.error("WebGL not supported");
    return;
  }
  const scale = 10;
  canvas.width = width * scale;
  canvas.height = height * scale;
  gl.viewport(0, 0, gl.drawingBufferWidth, gl.drawingBufferHeight);

  const program = createProgram(gl, vertexShaderSource, fragmentShaderSource);
  gl.useProgram(program);

  // Set up a full-screen quad.
  const positionBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
  const positions = new Float32Array([
    -1, -1,
     1, -1,
    -1,  1,
    -1,  1,
     1, -1,
     1,  1,
  ]);
  gl.bufferData(gl.ARRAY_BUFFER, positions, gl.STATIC_DRAW);
  const positionLocation = gl.getAttribLocation(program, "a_position");
  gl.enableVertexAttribArray(positionLocation);
  gl.vertexAttribPointer(positionLocation, 2, gl.FLOAT, false, 0, 0);

  const texCoordBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, texCoordBuffer);
  const texCoords = new Float32Array([
    0, 0,
    1, 0,
    0, 1,
    0, 1,
    1, 0,
    1, 1,
  ]);
  gl.bufferData(gl.ARRAY_BUFFER, texCoords, gl.STATIC_DRAW);
  const texCoordLocation = gl.getAttribLocation(program, "a_texCoord");
  gl.enableVertexAttribArray(texCoordLocation);
  gl.vertexAttribPointer(texCoordLocation, 2, gl.FLOAT, false, 0, 0);

  const texture = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, texture);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  const textureLocation = gl.getUniformLocation(program, "u_texture");
  gl.uniform1i(textureLocation, 0);

  let lastTime = performance.now();

  // Main render loop: run a cycle (or more) then update the texture from the Chip-8 screen buffer.
  function render() {
    const now = performance.now();
    const delta = now - lastTime; // time in ms
    lastTime = now;
    
    stats.begin();
    // Run one cycle per frame (adjust as needed).
    Module._run(10, delta);
    const screenPtr = Module._getScreen();
    const screenData = Module.HEAPU8.subarray(screenPtr, screenPtr + width * height);
    const imageData = new Uint8Array(width * height * 4);
    for (let i = 0; i < screenData.length; i++) {
      // Convert the 1-bit screen data to white/black pixels.
      const pixel = screenData[i] ? 255 : 0;
      imageData[i * 4 + 0] = pixel;
      imageData[i * 4 + 1] = pixel;
      imageData[i * 4 + 2] = pixel;
      imageData[i * 4 + 3] = 255;
    }
    gl?.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
    gl?.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, imageData);
    gl?.drawArrays(gl.TRIANGLES, 0, 6);
    stats.end();

    updateSound();
    
    requestAnimationFrame(render);
  }
  render();
}

main();