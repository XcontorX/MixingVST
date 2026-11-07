#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <cmath>

namespace mvst {

inline juce::String U(const char* s) { return juce::String::fromUTF8(s); }

struct FloatSpec { const char* id; const char* name; float mn, mx, def; const char* unit; int decimals; const char* tip; float centre; };
struct ToggleSpec { const char* id; bool def; const char* tip; };

inline const std::vector<FloatSpec>& floatSpecs() {
    static const std::vector<FloatSpec> s = {
        {"inGain","Entrada",-24,24,0," dB",1,"Sube o baja el volumen con el que la voz entra en la cadena. Busca que el medidor de entrada llegue a unos -12 dB sin tocar el rojo.",0},
        {"gateThr","Umbral",-80,-20,-62," dB",0,"Nivel por debajo del cual se silencia la señal. Quita respiraciones y ruido de fondo entre frases. Súbelo poco a poco hasta que corte el ruido sin comerse la voz.",0},
        {"gateRel","Cierre",20,500,150," ms",0,"Rapidez con la que la puerta se cierra tras cantar. Bajo = corte seco; alto = final de frase natural.",0},
        {"lowCut","Corte graves",20,300,80," Hz",0,"Elimina graves que no son voz: golpes de micro, aire, retumbos. 80-100 Hz es lo habitual; sube un poco en voces agudas.",100},
        {"mud","Barro 250",-10,3,-2," dB",1,"Reduce la zona de 250 Hz que hace la voz embarrada o tapada. Bájalo si suena confusa o con demasiado cuerpo.",0},
        {"box","Caja 500",-10,3,0," dB",1,"Reduce los 500 Hz, que dan sonido de cuarto pequeño o de hablar dentro de una caja. Bájalo si suena hueca.",0},
        {"dsFreq","Zona S",4000,10000,6500," Hz",0,"Frecuencia donde se detecta la letra S. Voces masculinas: 5-7 kHz. Voces femeninas: 7-9 kHz.",0},
        {"dsAmt","Cantidad",0,100,40," %",0,"Cuánto se suavizan las S y T agresivas (siseo). Si las eses pican, súbelo; si suena a ceceo, bájalo.",0},
        {"atAmt","Intensidad",0,100,70," %",0,"Cuánto se corrige la afinación. 0% = tu voz original; 100% = totalmente pegada a las notas de la escala.",0},
        {"atSpd","Velocidad",0,100,40," %",0,"Qué tan rápido salta a la nota correcta. 0% = efecto robot (trap / T-Pain); 100% = corrección lenta y natural.",0},
        {"c1Thr","Umbral",-40,0,-18," dB",1,"Nivel desde el que el compresor empieza a bajar los picos. Más bajo = comprime más. Busca 3-6 dB de reducción en el medidor.",0},
        {"c1Rat","Ratio",1,10,3,":1",1,"Intensidad de la compresión. 2:1 suave, 4:1 firme, 8:1 muy fuerte.",0},
        {"c1Atk","Ataque",1,60,20," ms",0,"Tiempo que tarda en actuar. Lento (20-30 ms) deja pasar el golpe de las sílabas; rápido las aplasta.",0},
        {"c1Rel","Relajación",30,500,150," ms",0,"Tiempo que tarda en soltar la compresión. Ajústalo para que respire con el ritmo de la canción.",0},
        {"c2Thr","Umbral",-40,0,-12," dB",1,"Segundo compresor, más rápido: atrapa los picos que se escapan del primero y deja la voz estable y al frente.",0},
        {"c2Rat","Ratio",1,20,4,":1",1,"Fuerza del compresor rápido. Valores altos dan voz muy constante y agresiva (rap, pop comercial).",0},
        {"satDrv","Calidez",0,100,20," %",0,"Añade saturación suave (armónicos) que da cuerpo, brillo y presencia. Demasiado puede sonar sucio.",0},
        {"warm","Graves 120",-6,6,0," dB",1,"Más cuerpo y calidez (positivo) o más ligereza (negativo) en los graves de la voz.",0},
        {"pres","Presencia",-6,6,1.5," dB",1,"Realza la zona de 3.5 kHz para que la voz se entienda y destaque sobre la instrumental.",0},
        {"air","Aire",0,10,3," dB",1,"Añade brillo y aire en los agudos (12 kHz), como en las voces de estudio profesional.",0},
        {"revMix","Mezcla",0,60,12," %",0,"Cantidad de reverb (eco de sala). Poco = voz cercana; mucho = voz lejana y espacial.",0},
        {"revSize","Tamaño",0,100,50," %",0,"Tamaño de la sala. Pequeño = cuarto o estudio; grande = catedral.",0},
        {"revDamp","Oscuridad",0,100,50," %",0,"Apaga los agudos de la reverb. Más alto = reverb más oscura y suave, con menos siseo.",0},
        {"dlyMix","Mezcla",0,60,10," %",0,"Volumen de los ecos. Un valor sutil (5-12%) añade profundidad sin ensuciar.",0},
        {"dlyFb","Repeticiones",0,80,30," %",0,"Cuántas veces se repite el eco antes de desaparecer.",0},
        {"outGain","Salida",-12,12,0," dB",1,"Volumen final de la voz. Ajusta para que encaje con la instrumental.",0},
        {"ceil","Techo",-6,0,-1," dB",1,"Límite máximo de volumen (limitador final). -1 dB es el estándar seguro para Spotify y YouTube.",0},
    };
    return s;
}

inline const std::vector<ToggleSpec>& toggleSpecs() {
    static const std::vector<ToggleSpec> s = {
        {"gateOn",true,"Activa o desactiva la puerta de ruido. Verde = encendido."},
        {"eqOn",true,"Activa o desactiva el EQ de limpieza. Verde = encendido."},
        {"dsOn",true,"Activa o desactiva el De-Esser. Verde = encendido."},
        {"atOn",true,"Activa o desactiva el Autotune. Verde = encendido."},
        {"c1On",true,"Activa o desactiva el compresor 1. Verde = encendido."},
        {"c2On",true,"Activa o desactiva el compresor 2. Verde = encendido."},
        {"satOn",true,"Activa o desactiva la saturación. Verde = encendido."},
        {"tonOn",true,"Activa o desactiva el EQ de tono. Verde = encendido."},
        {"revOn",true,"Activa o desactiva la reverb. Verde = encendido."},
        {"dlyOn",true,"Activa o desactiva el delay. Verde = encendido."},
    };
    return s;
}

inline juce::StringArray keyNames()   { return { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }; }
inline juce::StringArray scaleNames() { return { U("Cromática"), "Mayor", "Menor", "Pent. Menor", "Pent. Mayor", "Blues" }; }
inline juce::StringArray divNames()   { return { "1/4", "1/8", "1/8 con punto", "1/16" }; }

inline std::vector<bool> scaleMask(int idx) {
    static const std::vector<std::vector<int>> iv = {
        {0,1,2,3,4,5,6,7,8,9,10,11}, {0,2,4,5,7,9,11}, {0,2,3,5,7,8,10}, {0,3,5,7,10}, {0,2,4,7,9}, {0,3,5,6,7,10} };
    std::vector<bool> m(12, false);
    for (int i : iv[(size_t) juce::jlimit(0, (int) iv.size() - 1, idx)]) m[(size_t) i] = true;
    return m;
}

struct Preset { const char* name; std::vector<std::pair<const char*, float>> v; };
inline const std::vector<Preset>& presets() {
    static const std::vector<Preset> p = {
        {"Limpia (por defecto)", {}},
        {"Pop Moderno", {{"atAmt",60},{"atSpd",35},{"c1Thr",-20},{"satDrv",20},{"pres",2},{"air",4},{"revMix",14},{"revSize",45},{"dlyMix",8}}},
        {"Rap / Trap", {{"atAmt",85},{"atSpd",10},{"lowCut",90},{"c1Thr",-22},{"c1Rat",4},{"c2Thr",-14},{"c2Rat",6},{"satDrv",35},{"pres",2.5f},{"air",3},{"revMix",8},{"revSize",30},{"dlyMix",14},{"dlyFb",35}}},
        {"R&B Suave", {{"atAmt",45},{"atSpd",55},{"c1Rat",2.5f},{"warm",1.5f},{"pres",1},{"air",3},{"revMix",22},{"revSize",65},{"revDamp",60},{"dlyMix",12}}},
        {"Rock Potente", {{"atAmt",15},{"atSpd",70},{"c1Thr",-22},{"c1Rat",4},{"c2Thr",-14},{"c2Rat",5},{"satDrv",50},{"pres",3},{"air",2},{"revMix",10},{"revSize",40},{"dlyMix",4}}},
        {"Podcast / Hablada", {{"atOn",0},{"lowCut",100},{"mud",-4},{"c1Thr",-20},{"satDrv",10},{"pres",2},{"air",1},{"revOn",0},{"dlyOn",0}}},
    };
    return p;
}

} // namespace mvst
