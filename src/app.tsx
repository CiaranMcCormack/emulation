import { useState } from 'react'
import Chip8 from './emulators/chip8'
import { Box } from '@mui/material'
import FileLoader from './fileLoader'

export default function App() {
  const [rom, setRom] = useState<Uint8Array<ArrayBuffer> | null>(null)

  if (rom === null) {
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
        <FileLoader onSelect={file => {
          const reader = new FileReader()
          reader.onload = () => {
            setRom(new Uint8Array(reader.result as ArrayBuffer))
          }
          reader.readAsArrayBuffer(file)
        }} />
      </Box>
    )
  }

  return <Chip8 rom={rom} />
}