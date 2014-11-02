import Graphics.Input as Input
import FileReader
import String
import Maybe

files : Input.Input (Maybe FileReader.Blob)
files = Input.input Nothing

flines : Signal (Maybe [String])
flines = FileReader.results >> Maybe.map String.lines <~ FileReader.readAsText files.signal

main = (\mflines ->
    flow down [
        FileReader.fileInput files.handle <| Maybe.map FileReader.asBlob,
        Input.button files.handle Nothing "Zig-Zag",
        asText <| mflines
    ]) <~ flines
