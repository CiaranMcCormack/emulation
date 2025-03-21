import Button from '@mui/material/Button'
import UploadFileIcon from '@mui/icons-material/UploadFile'

function FileLoader({ onSelect }: { onSelect: (file: File) => void }) {
  return (
    <Button
      variant="contained"
      startIcon={<UploadFileIcon />}
      component="label"
    >
      Load ROM
      <input
        type="file"
        accept=".ch8"
        hidden
        onChange={e => {
          const file = e.target.files?.[0]
          if (file) onSelect(file)
        }}
      />
    </Button>
  )
}

export default FileLoader;