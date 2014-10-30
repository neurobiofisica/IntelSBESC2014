import Graphics.Input as Input
import WebSocket
import Bitwise
import Window
import Maybe
import Text
import Json
import List
import Dict

type DataList = [(Int,Float)]

lastStim : Signal (Maybe [Int])
lastStim = extractStim <~ keepIf isStim Nothing socket

dataListFromJson : Maybe Json.Value -> DataList
dataListFromJson json = case json of
    Just (Json.Array arr) -> List.filterMap (\elem -> case elem of
        Json.Array ([Json.Number x, Json.Number y]) -> Just (truncate x, y)
        _ -> Nothing) arr
    _ -> []

dicFromJson : Maybe Json.Value -> Maybe (Dict.Dict String Json.Value)
dicFromJson json = case json of
    Just (Json.Object dic) -> Just dic
    _ -> Nothing

checkKind : String -> Maybe Json.Value -> Bool
checkKind kind json = dicFromJson json
    |> Maybe.map (\dic -> Dict.get "kind" dic == Just (Json.String kind))
    |> (==) (Just True)

isStim : Maybe Json.Value -> Bool
isStim = checkKind "stim"

clearReqs : Signal Int
clearReqs = countIf (checkKind "clear") socket

extractStim : Maybe Json.Value -> Maybe [Int]
extractStim json = case dicFromJson json of
    Just dic -> case Dict.get "data" dic of
        Just (Json.Array arr) -> Just <| List.filterMap (\elem -> case elem of
            Json.Number x -> Just (truncate x)
            _ -> Nothing) arr
        _ -> Nothing
    _ -> Nothing

socket : Signal (Maybe Json.Value)
socket = Json.fromString <~
    (WebSocket.connect "ws://192.168.0.1:8888/socket"
    <| constant "")

timesFromData : Int -> DataList -> [(Bool,Float)]
timesFromData ch list =
    let chmask   = 1 `Bitwise.shiftLeft` ch
        feedmask = 1 `Bitwise.shiftLeft` (numChannels + 1) 
    in List.filterMap (\(flags, time) ->
        if | flags `Bitwise.and` chmask == 0 -> Nothing 
           | otherwise -> Just (flags `Bitwise.and` feedmask /= 0, time)
       ) list

type PlotFunc = (Int,Int) -> [(Bool,Float)] -> [Form]

type PlotInput = {
    ch:Int,            -- data channel to plot
    dim:(Int,Int),     -- plot area dimensions
    drawtime:Time,     -- from Time.every or Time.fps
    incoming:DataList,
    ticket:Int,        -- ticket for incoming changes
    clreq:Int          -- ticket for clear requests
}

type PlotState = {
    ch:Int,
    dim:(Int,Int),
    pending:DataList,
    archive:DataList,
    drawtime:Time,
    ticket:Int,
    clreq:Int,
    plot:[Form]
}

initialPlotState : PlotState
initialPlotState = { ch = 0, dim = (0,0), pending = [], archive = [],
    drawtime = 0, ticket = 0, clreq = 0, plot = [] }

updatePlot : PlotFunc -> PlotInput -> PlotState -> PlotState
updatePlot func inp cur =
    let haveIncoming = inp.ticket /= cur.ticket
        clearRequested = inp.clreq /= cur.clreq
        redraw = inp.ch /= cur.ch || inp.dim /= cur.dim || clearRequested
        draw = redraw || inp.drawtime /= cur.drawtime
        pending = if haveIncoming
                     then inp.incoming ++ cur.pending
                     else cur.pending
        archive = if | clearRequested -> []
                     | draw -> pending ++ cur.archive
                     | otherwise -> cur.archive
        list = if | redraw -> archive
                  | draw -> pending
                  | otherwise -> []
        curplot = if | redraw -> []
                     | otherwise -> cur.plot
        times = timesFromData inp.ch list
        plot = if | draw -> [func inp.dim times |> (++) curplot |> group]
                  | otherwise -> cur.plot
    in { ch = inp.ch,
         dim = inp.dim,
         pending = if draw then [] else pending,
         archive = archive,
         drawtime = inp.drawtime,
         ticket = inp.ticket,
         clreq = inp.clreq,
         plot = plot }

plotRaster : PlotFunc
plotRaster (w,h) times =
    let vspacing = 5.0
        xrange = 5.0
        border = 5.0
        w' = (toFloat w) - 2*border
        xoff = border - (toFloat w)/2
        yoff = border - (toFloat h)/2
    in List.map (\(feedactive, time) ->
        let lineno = time / xrange |> floor |> toFloat
            y = yoff + vspacing*lineno
            x = xoff + w' * (time/xrange - lineno)
            pcolor = if feedactive then red else black
        in square 2 |> filled pcolor |> move (x,y)
    ) times

plotStim : (Int,Int) -> Maybe [Int] -> Element
plotStim (w,h) mstim = case mstim of
    Nothing -> empty
    Just stim -> collage w h <|
        let border = 5.0
            w' = (toFloat w) - 2*border
            h' = (toFloat h) - 2*border
            xoff = border - (toFloat w)/2
            yoff = border - (toFloat h)/2
            xfac = w' / (length stim |> toFloat)
            yfac = h' / 8.0
        in List.indexedMap(\i elem ->
            let x = xoff + xfac*(toFloat i)
            in [0..7] |> List.filterMap (\j ->
                let mask = 1 `Bitwise.shiftLeft` (truncate j)
                    y = yoff + yfac*j
                in if elem `Bitwise.and` mask /= 0
                      then square 2 |> filled black |> move (x,y) |> Just
                      else Nothing
            ) |> group
        ) stim

plotDim : Signal (Int,Int)
plotDim = (\(w,h) -> 
    (w - contBorder, h - 2*contBorder - stimPlotHeight - chSelHeight))
    <~ Window.dimensions

plotInput : Signal PlotInput
plotInput = 
    let incomingSig = dataListFromJson <~ socket
        ticketSig = count incomingSig
    in (\ch dim drawtime incoming ticket clreq ->
            { ch = ch, dim = dim, drawtime = drawtime,
              incoming = incoming, ticket = ticket,
              clreq = clreq })
       <~ channelInput.signal ~ plotDim ~ (every second)
        ~ incomingSig ~ ticketSig ~ clearReqs

stimPlot : Signal Element
stimPlot = (\(w,h) stim -> plotStim (w - contBorder, stimPlotHeight) stim)
    <~ Window.dimensions ~ lastStim

plotContainer : Int -> Int -> Element -> Element
plotContainer w h elem = elem
    |> container w h middle
    |> color lightGray
    |> container (w + contBorder) (h + contBorder) middle
    |> color darkGray

channelInput : Input.Input Int
channelInput = Input.input 0

numChannels : Int
numChannels = 8

chSelScene : Int -> Int -> Int -> Element
chSelScene w h sel = [ [0..numChannels-1]
    |> List.map (\ch -> [spacer 10 h, chButton ch (ch==sel)])
    |> List.concat
    |> flow right ]
    |> (::) (spacer w contBorder)
    |> flow down
    |> container w h midLeft
    |> color darkGray

chButton : Int -> Bool -> Element
chButton ch isSel =
    let bcolor = if | isSel -> rgb 200 255 200
                    | otherwise -> lightGray
        n = chSelHeight - contBorder
        btn alpha = 
            layers [ container n n middle (txt (0.6*n) black (show ch))
                     |> container (n-2) (n-2) middle
                     |> color bcolor
                     |> container n n middle
                     |> color black,
                     color (rgba 0 0 0 alpha) (spacer n n) ]
    in Input.customButton channelInput.handle ch (btn 0) (btn 0.05) (btn 0.1)

txt : Float -> Color -> String -> Element
txt h clr string =
    toText string
    |> Text.color clr
    |> typeface ["Helvetica Neue","Sans-serif"]
    |> Text.height h
    |> leftAligned

chSelHeight : number
chSelHeight = 50

stimPlotHeight : number
stimPlotHeight = 21

contBorder : number
contBorder = 10

chSelector : Signal Element
chSelector = (\(w,h) ch -> chSelScene (w + contBorder) chSelHeight ch)
    <~ plotDim ~ channelInput.signal

main : Signal Element
main = (\(w,h) st stimp chsel ->
   flow down [ chsel,
       stimp |> plotContainer w stimPlotHeight,
       collage w h st.plot |> plotContainer w h ])
  <~ plotDim ~ foldp (updatePlot plotRaster) initialPlotState plotInput 
   ~ stimPlot ~ chSelector
