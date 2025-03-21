import React, { useRef, useEffect, useState, useCallback, } from 'react'
import Stats from 'stats.js'

import Box from '@mui/material/Box'
import FormControlLabel from '@mui/material/FormControlLabel'
import Checkbox from '@mui/material/Checkbox'
import { createProgram, setupBuffers } from '../utils/graphics'
import { useEvent } from '../utils/hooks'

declare var Module: any

const chip8KeyMap: Record<string, number> = {
  Digit1: 0x1, Digit2: 0x2, Digit3: 0x3, Digit4: 0xC,
  KeyQ: 0x4, KeyW: 0x5, KeyE: 0x6, KeyR: 0xD,
  KeyA: 0x7, KeyS: 0x8, KeyD: 0x9, KeyF: 0xE,
  KeyZ: 0xA, KeyX: 0x0, KeyC: 0xB, KeyV: 0xF
}

interface Props {
  rom: Uint8Array<ArrayBuffer>
}

const Chip8: React.FC<Props> = ({ rom }) => {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const oscillatorRef = useRef<OscillatorNode | null>(null)
  const [soundEnabled, setSoundEnabled] = useState(true)

  const soundEnabledRef = useRef(soundEnabled)
  useEffect(() => {
    soundEnabledRef.current = soundEnabled
  }, [soundEnabled])
  
  const stats = new Stats()
  const audioCtx = new AudioContext()

  const updateSound = useEvent(() => {
    const st = Module._getSoundTimer()

    // If sound is enabled *and* timer > 0, ensure we have an oscillator
    if (soundEnabledRef.current && st > 0) {
      if (!oscillatorRef.current) {
        const osc = audioCtx.createOscillator()
        osc.type = 'square'
        osc.frequency.value = 440
        osc.connect(audioCtx.destination)
        osc.start()
        oscillatorRef.current = osc
      }
    } 
    // Otherwise stop any existing oscillator
    else if (oscillatorRef.current) {
      oscillatorRef.current.stop()
      oscillatorRef.current.disconnect()
      oscillatorRef.current = null
    }
  })

  useEffect(() => {
    stats.showPanel(0)
    stats.dom.style.position = 'absolute'
    stats.dom.style.top = '0'
    stats.dom.style.right = '0'
    document.body.appendChild(stats.dom)

    const handleKey = (fn: string) => (e: KeyboardEvent) => {
      const key = chip8KeyMap[e.code]
      if (key !== undefined) Module[`_${fn}`](key)
    }
    document.addEventListener('keydown', handleKey('setKeyDown'))
    document.addEventListener('keyup', handleKey('setKeyUp'))

    

    const init = async () => {
      if (!Module.calledRun) {
        await new Promise(resolve => { Module.onRuntimeInitialized = resolve })
      }
      Module._init()

      const ptr = Module._malloc(rom.length)
      Module.HEAPU8.set(rom, ptr)
      Module._loadProgram(ptr, rom.length)
      Module._free(ptr)
     
      const gl = canvasRef.current!.getContext('webgl')!
      const width = Module._getScreenWidth()
      const height = Module._getScreenHeight()
      canvasRef.current!.width = width * 10
      canvasRef.current!.height = height * 10
      gl.viewport(0, 0, gl.canvas.width, gl.canvas.height)

      const program = createProgram(gl);
      gl.useProgram(program)
      setupBuffers(gl, program)

      let last = performance.now()
      const loop = () => {
        stats.begin()
        const now = performance.now()
        const delta = now - last
        last = now

        Module._run(10, delta)

        const screenPtr = Module._getScreen()
        const pixels = new Uint8Array(Module.HEAPU8.buffer, screenPtr, width * height)
        const img = new Uint8Array(width * height * 4)
        for (let i = 0; i < pixels.length; i++) {
          const v = pixels[i] ? 255 : 0
          img[i*4] = v; img[i*4+1] = v; img[i*4+2] = v; img[i*4+3] = 255
        }
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true)
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, img)
        gl.drawArrays(gl.TRIANGLES, 0, 6)

        updateSound()
        stats.end()
        requestAnimationFrame(loop)
      }
      requestAnimationFrame(loop)
    }
    init()

    return () => {
      document.body.removeChild(stats.dom)
      document.removeEventListener('keydown', handleKey('setKeyDown'))
      document.removeEventListener('keyup', handleKey('setKeyUp'))
    }
  }, [])

  return (
    <Box
      sx={{
        display: 'flex',
        flexDirection: 'column',
        justifyContent: 'center',
        alignItems: 'center',
        height: '100vh',    // fill viewport
        gap: 2,             // spacing between children
        p: 4,
      }}
    >
      <FormControlLabel
  control={
    <Checkbox
      checked={soundEnabled}
      onChange={() => setSoundEnabled(enabled => !enabled)}
    />
  }
  label="Sound"
/>
      <canvas id="glCanvas" ref={canvasRef}></canvas>
    </Box>
  )
}

export default Chip8;