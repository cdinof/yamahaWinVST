# 🎹 Yamaha Arranger — plugin VST3 (FL Studio)

Plugin que **toca estilos Yamaha (.sty)** dentro do FL Studio: você toca os
acordes no teclado e o acompanhamento (bateria, baixo, piano, guitarra, cordas…)
sai sozinho, reharmonizado — como um arranjador Yamaha de verdade.

```
Teclado MIDI ──▶ FL Studio ──▶ [Yamaha Arranger] ──▶ áudio (SoundFont .sf2)
                    (host)        detecta acorde       bateria + baixo +
                                    + toca o .sty      piano + guitarra...
```

---

## ✨ O que ele faz

| Recurso | |
|---|---|
| Lê arquivos `.sty` (SFF1 e SFF2) | ✅ seções, tempo, compasso |
| Decodifica o **CASM** completo | ✅ Ctab, Ctb2 (3 zonas), Cntt |
| Transposição **NTR/NTT** | ✅ Bypass, Melody, Chord, Bass, menores, Dorian |
| **Padrões alternativos maior/menor** | ✅ via `chordMute` (o segredo dos estilos modernos) |
| Canal de **destino** (9–16) | ✅ roteamento igual ao teclado |
| Note Limit / High Key / Bass On | ✅ |
| Bateria **nunca** transposta | ✅ detectada por Bank MSB, nome e convenção |
| Detecta o padrão de som | ✅ GM1/GM2/GS/XG/MT-32/GM Lite/MIDI 2.0 |
| Sintetizador SoundFont embutido | ✅ (TinySoundFont, domínio público) |
| Painel estilo Yamaha | ✅ display, seções, transporte, 4 knobs, 16 canais |
| Fila de seções | ✅ Intro A → Main C antes do START; Fill B → Main C durante |

---

## 🚀 Como usar (não precisa saber programar)

### 1. Baixar o plugin pronto

Toda vez que o código é atualizado, o **GitHub compila sozinho** e publica o
arquivo pronto:

1. Abra a aba **Actions** do repositório
2. Clique no build mais recente (ou em **Run workflow**)
3. Baixe o artefato **`YamahaArranger-Windows`** (ou macOS)
4. Descompacte

### 2. Instalar no Windows

Copie a pasta `YamahaArranger.vst3` para:

```
C:\Program Files\Common Files\VST3
```

No FL Studio: **Options → File settings → Manage plugins** → *Find installed
plugins* → *Scan*. O plugin aparece como **Yamaha Arranger**.

### 3. Usar

1. Arraste o **Yamaha Arranger** para um canal do mixer
2. No painel do plugin, clique em **SOUNDFONT** e escolha um `.sf2`
   (qualquer SoundFont GM — ex.: FluidSynth's `FluidR3_GM.sf2`)
3. Clique em **STYLE** e escolha um `.sty` da Yamaha
4. Toque acordes no teclado **abaixo de F#3** (mão esquerda) — o display mostra
   o acorde detectado
5. Clique em **START**

> A mão direita (acima de F#3) passa direto para o sintetizador, então você se
> ouve tocando a melodia por cima do acompanhamento.

---

## 🔧 Compilar você mesmo (opcional)

Só se quiser. Precisa de **CMake 3.22+** e de um compilador C++ (Visual Studio
2022 no Windows, Xcode no macOS). **Não precisa instalar JUCE** — o CMake baixa
sozinho.

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Saídas em `build/YamahaArranger_artefacts/Release/`:
- `VST3/YamahaArranger.vst3` — o plugin
- `Standalone/YamahaArranger.exe` — app avulso para testar sem FL Studio

Rodar os testes do motor:

```bash
cmake --build build --target engine_test
./build/Release/engine_test        # Windows
./build/engine_test                # Linux/macOS
```

---

## 📂 Estrutura

```
engine/            motor (C++ puro, sem dependências)
  YamahaTypes.h      tabelas de chord type + enums do CASM
  StyleModel.h       modelos (eventos, zonas, regras, seções, estilo)
  Chord.h/.cpp       acorde + qualidades
  ChordDetector.*    detecção estilo Yamaha (Single Finger, Fingered, AI)
  ChordTransposer.*  regras NTR/NTT
  StyParser.*        leitor .sty (SMF + CASM)
  StylePlayer.*      sequenciador (seções, filas, roteamento)
plugin/            camada do plugin (JUCE)
  PluginProcessor.*  áudio + MIDI do host
  PluginEditor.*     painel
  TsfSynth.*         sintetizador SoundFont embutido
  PluginEntry.cpp    createPluginFilter()
third_party/tsf.h  TinySoundFont (domínio público)
tests/             testes do motor + amostra .sty
.github/workflows/ build automático (Windows + macOS)
```

---

## 📄 Detalhes técnicos do formato

O CASM dos estilos Yamaha está documentado em
[`docs/casm_layout.md`](docs/casm_layout.md) (mesmo doc do app Android):
layout byte a byte do Ctab/Ctb2/Cntt, NTR/NTT, note/chord mute e os padrões
alternativos maior/menor.

---

## ⚖️ Licenças

- Motor e plugin: **MIT**
- `third_party/tsf.h` (TinySoundFont): **domínio público**
- JUCE: **AGPLv3 / comercial** (baixado automaticamente pelo CMake)

> Marcas registradas: Yamaha, General MIDI, XG e GS pertencem aos seus
> detentores. Projeto independente, sem afiliação.
