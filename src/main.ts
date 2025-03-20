// Import stats.js (ES module version)
import Stats from 'stats.js';

// Declare the Module global variable provided by chip8.js
declare var Module: any;

console.log("Hello from main.ts");
console.log("Module global:", Module);

// Create a stats instance and add it to the DOM.
const stats = new Stats();
// 0: fps, 1: ms, 2: mb (if supported)
// You can switch panels if needed. For example: stats.showPanel(0);
stats.showPanel(0);
document.body.appendChild(stats.dom);

// Reposition the stats panel to the top right.
stats.dom.style.left = 'auto';
stats.dom.style.right = '0px';
stats.dom.style.top = '0px';

// Helper function: creates and compiles a shader.
function createShader(
  gl: WebGLRenderingContext,
  source: string,
  type: number
): WebGLShader {
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

// Helper function: links vertex and fragment shaders into a program.
function createProgram(
  gl: WebGLRenderingContext,
  vsSource: string,
  fsSource: string
): WebGLProgram {
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

// Define shader sources.
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

// Main initialization function.
function initChip8() {
  console.log("WASM Module Initialized");
  Module._init_chip8();

  const width: number = Module._getScreenWidth();
  const height: number = Module._getScreenHeight();
  console.log("Screen dimensions:", width, height);

  const canvas = document.getElementById("glCanvas") as HTMLCanvasElement;
  if (!canvas) {
    console.error("Canvas element not found");
    return;
  }
  const gl = canvas.getContext("webgl");
  if (!gl) {
    console.error("WebGL not supported");
    return;
  }
  console.log("WebGL context obtained");

  // Scale canvas for visibility.
  const scale = 10;
  canvas.width = width * scale;
  canvas.height = height * scale;
  gl.viewport(0, 0, gl.drawingBufferWidth, gl.drawingBufferHeight);

  // Create and use the shader program.
  const program = createProgram(gl, vertexShaderSource, fragmentShaderSource);
  gl.useProgram(program);
  console.log("Shaders compiled and program linked");

  // Set up vertex positions for a full-screen quad.
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
  if (positionLocation === -1) {
    console.error("Could not find attribute a_position");
  }
  gl.enableVertexAttribArray(positionLocation);
  gl.vertexAttribPointer(positionLocation, 2, gl.FLOAT, false, 0, 0);

  // Set up texture coordinates.
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
  if (texCoordLocation === -1) {
    console.error("Could not find attribute a_texCoord");
  }
  gl.enableVertexAttribArray(texCoordLocation);
  gl.vertexAttribPointer(texCoordLocation, 2, gl.FLOAT, false, 0, 0);

  // Create a texture to hold the CHIP-8 screen data.
  const texture = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, texture);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  const textureLocation = gl.getUniformLocation(program, "u_texture");
  gl.uniform1i(textureLocation, 0);

  // Render loop.
  function render() {
    stats.begin(); // Start stats measurement.

    Module._updateScreen();
    const screenPtr = Module._getScreen();
    const screenData = Module.HEAPU8.subarray(screenPtr, screenPtr + width * height);
    const imageData = new Uint8Array(width * height * 4);
    for (let i = 0; i < screenData.length; i++) {
      const value = screenData[i] * 255; // 0 or 255.
      imageData[i * 4 + 0] = value;
      imageData[i * 4 + 1] = value;
      imageData[i * 4 + 2] = value;
      imageData[i * 4 + 3] = 255;
    }
    gl?.texImage2D(
      gl.TEXTURE_2D,
      0,
      gl.RGBA,
      width,
      height,
      0,
      gl.RGBA,
      gl.UNSIGNED_BYTE,
      imageData
    );
    gl?.drawArrays(gl.TRIANGLES, 0, 6);

    stats.end(); // End stats measurement.
    requestAnimationFrame(render);
  }
  render();
}

// If the module is already initialized, run immediately; otherwise, use the callback.
if (Module.calledRun) {
  initChip8();
} else {
  Module.onRuntimeInitialized = initChip8;
}