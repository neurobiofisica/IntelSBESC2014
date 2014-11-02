import Graphics.Input.Field as Field
import Graphics.Input as Input
import FileReader
import Window
import String
import Signal
import Maybe
import Char
import List
import Json
import Http
import Text

stimInput defv =
    let inp = Input.input defv
    in { inp | contents = 
         FileReader.results
         >> Maybe.map (String.lines
             >> List.filterMap String.toInt)
         <~ FileReader.readAsText inp.signal }

intArrToJson : [Int] -> Json.Value
intArrToJson = List.map (toFloat >> Json.Number) >> Json.Array

jsonToStr : Maybe Json.Value -> String
jsonToStr mjson = case mjson of
    Nothing  -> ""
    Just json -> Json.toString "" json

put : Signal (String, Maybe Json.Value) -> Signal Element
put sig = (\(url, json) response ->
    if | url == "" -> empty
       | otherwise -> display response)
    <~ sig
     ~ ((\(url, json) ->
        Http.request "PUT" url (jsonToStr json)
            [("Content-Type", "application/json")]) <~ sig
        |> Http.send)

display : Http.Response String -> Element
display response =
    case response of
        Http.Success _   -> spacer 16 16
        Http.Waiting     -> image  16 16 "waiting.gif"
        Http.Failure _ _ -> show response |> txt 11 black
            |> container ifcWidth 16 middle
            |> color lightRed 

putIntArr : String -> Signal (Maybe [Int]) -> Signal Element
putIntArr url sig = (\arr ->
    (Maybe.maybe "" (always url) arr,
     Maybe.map intArrToJson arr))
    <~ sig |> put

putCmd : Signal String -> Signal Element
putCmd sig = (\url -> (url, Nothing)) <~ sig |> put

putCmdArg : Signal (String, String) -> Signal Element
putCmdArg sig = (\(url, arg) ->
    (url,
        if | arg == "" -> Nothing
           | otherwise -> Just <| Json.String arg
    )) <~ sig |> put

fileInput : Input.Handle (Maybe FileReader.Blob) -> Element
fileInput handle = FileReader.fileInput handle <| Maybe.map FileReader.asBlob

button : Int -> Int -> Color -> String -> Input.Handle a -> a -> Element
button w h bcolor caption handle val =
    let h' = toFloat h
        btn alpha = 
            layers [ container w h middle (txt 12 black caption)
                     |> container (w-2) (h-2) middle
                     |> color bcolor
                     |> container w h middle
                     |> color black,
                     color (rgba 0 0 0 alpha) (spacer w h) ]
    in Input.customButton handle val (btn 0) (btn 0.05) (btn 0.1)

txt : Float -> Color -> String -> Element
txt h clr string =
    toText string
    |> Text.color clr
    |> typeface ["Helvetica Neue","Sans-serif"]
    |> Text.height h
    |> leftAligned

field : (String -> Bool) -> Input.Input Field.Content -> Signal Element
field allowed inp = (\content ->
        Field.field Field.defaultStyle inp.handle identity "" content
        |> color white
    ) <~ keepIf (.string >> allowed) Field.noContent inp.signal

longStim  = stimInput Nothing
briefStim = stimInput Nothing

cmd : Input.Input String
cmd = Input.input ""

binSize : Input.Input Field.Content
binSize = Input.input Field.noContent

wordToMatch : Input.Input Field.Content
wordToMatch = Input.input Field.noContent

cmdArg : Input.Input (String, String)
cmdArg = Input.input ("", "")

httpResp = Signal.merges [
        putIntArr "/control/longstim"  longStim.contents,
        putIntArr "/control/briefstim" briefStim.contents,
        putCmd cmd.signal,
        putCmdArg cmdArg.signal
    ]

charIn : String -> Char -> Bool
charIn s c = String.contains (String.cons c "") s

ifcWidth : number
ifcWidth = 400

main = (\(w,h) httpResp' binSizeElem binSize' wordToMatchElem wordToMatch' ->
    let bgray = rgb 220 220 220 in
    container w h middle <|
    flow down [
        httpResp' |> container ifcWidth 16 middle,
        color lightGray <| container ifcWidth 50 middle <| flow right [
            button 80 30 (rgb 180 255 180) "Start" cmd.handle "/control/start",
            spacer 80 10,
            button 80 30 (rgb 255 180 180) "Stop"  cmd.handle "/control/stop"
        ],
        spacer ifcWidth 10,
        color lightGray <| container ifcWidth 130 middle <| flow down [
            txt 16 black "Long stimuli" |> container ifcWidth 28 middle,
            fileInput longStim.handle   |> container ifcWidth 30 middle,
            txt 13 black "or"           |> container ifcWidth 20 middle,
            button 80 30 bgray "Zig-Zag" cmd.handle "/control/zigzag"
                |> container ifcWidth 40 middle
        ],
        spacer ifcWidth 10,
        color lightGray <| container ifcWidth 220 middle <| flow down [
            txt 16 black "Pattern matcher" |> container ifcWidth 28 middle,
            txt 13 black "Feedback (brief) stimuli"
                |> container ifcWidth 20 middle,
            fileInput briefStim.handle     |> container ifcWidth 30 middle,
            txt 13 black "Bin size (units of stimuli period)"
                |> container ifcWidth 20 middle,
            flow right [
                binSizeElem |> container 200 30 middle,
                button 30 30 bgray "Set" cmdArg.handle ("/control/binsize", binSize'.string)
            ] |> container ifcWidth 30 middle,
            txt 13 black "Word to match"
                |> container ifcWidth 20 middle,
            flow right [
                wordToMatchElem |> container 200 30 middle,
                button 30 30 bgray "Set" cmdArg.handle ("/control/word", wordToMatch'.string)
            ] |> container ifcWidth 30 middle,
            button 60 30 bgray "Disable" cmd.handle "/control/pattern_disable"
                |> container ifcWidth 40 middle
        ]
    ]) <~ Window.dimensions ~ httpResp
        ~ field (String.all Char.isDigit ) binSize     ~ binSize.signal
        ~ field (String.all (charIn "01")) wordToMatch ~ wordToMatch.signal
