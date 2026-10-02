# CASM — layout do bloco (SFF1 / SFF2)

O **CASM** é o bloco que vem **depois** da trilha MIDI (MTrk) num arquivo
`.sty` do Yamaha. É ele que diz ao arranjador **como converter** o padrão
gravado (sempre em **CMaj7**, por padrão) para o acorde que o músico está
tocando. Sem ele, o estilo é só um MIDI qualquer — e soa errado.

```
CASM (len)
├── CSEG (len)                    # um grupo de seções com regras iguais
│   ├── Sdec (len) "Main A"       # nomes das seções cobertas, separados por vírgula
│   ├── Ctab (len) <27 bytes>     # regra por canal de origem  (SFF1)
│   ├── Ctb2 (len) <≥40 bytes>    # regra por canal, com 3 zonas (SFF2)
│   └── Cntt (len) <2 bytes>      # refina a tabela NTT de um Ctab
└── … (outros CSEG)
```

## Ctab (SFF1) — 27 bytes

| Off | Campo | Valores | Significado |
|---|---|---|---|
| 0 | Source Channel | 00–0F | canal da trilha que contém as notas (**0-based**) |
| 1–8 | Voice Name | ASCII | nome da parte ("E Bass", "AD Dance"…) |
| 9 | Destination Channel | 08–0F | canal de destino (**0-based**, 8..15 = canais 9–16) |
| 10 | Editable | 00–01 | 00 = editável, 01 = só leitura |
| 11–12 | Note Mute | nibbles | quais **tônicas** tocam (codificação Gray) |
| 13–17 | Chord Mute | nibbles | quais **tipos de acorde** tocam; bit 34 = autostart |
| 18 | Source Root | 00–0B | tônica do padrão gravado (0 = C) |
| 19 | Source Chord Type | 00–21 | tipo do acorde gravado (02 = M7) |
| 20 | **NTR** | 00–01(02) | 00 ROOT TRANS, 01 ROOT FIXED, 02 Guitar |
| 21 | **NTT** | 00–05 | 00 Bypass, 01 Melody, 02 Chord, 03 Bass, 04 Melodic Minor, 05 Harmonic Minor |
| 22 | High Key | 00–0B | limite de tônica (só no ROOT TRANS) |
| 23 | Note Low Limit | 00–7F | limite inferior de nota |
| 24 | Note High Limit | 00–7F | limite superior de nota |
| 25 | RTR | 00–05 | 00 Stop, 01 Pitch Shift, 02 Pitch Shift to Root, 03 Retrigger, 04 Retrigger to Root, 05 Note Generator |
| 26 | End Marker | 00 | fim do registro |

## Ctb2 (SFF2) — ≥ 40 bytes

Os 22 primeiros bytes são iguais aos do Ctab; a partir daí:

| Off | Campo |
|---|---|
| 20 | mid_lo — notas abaixo usam a zona 0 |
| 21 | mid_hi — notas acima usam a zona 2 |
| 22–27 | zona 0: `[NTR, NTT, high key, note low, note high, RTR]` |
| 28–33 | zona 1 |
| 34–39 | zona 2 |

Na zona do Ctb2 o **bit 7 do byte NTT** é o *Bass On* (segue acorde com baixo
diferente). No Ctab SFF1 não existe bit 7: *Bass On* vem do próprio código
`03` (**Bass**).

## Cntt — 2 bytes

| Off | Campo |
|---|---|
| 0 | Source Channel (0-based) |
| 1 | NTT (tabelas 00–0A: Bypass, Melody, Chord, Melodic Min, Melodic Min 5ª, Harmonic Min, Harmonic Min 5ª, Natural Min, Natural Min 5ª, Dorian, Dorian 5ª) |

Só refina um **Ctab** (o Ctb2 já traz NTT por zona). O *Bass On* é somado
(`OR`), nunca substituído.

## Como o player usa

1. **Canal de destino**: as partes de acompanhamento vivem nos canais 9–16.
   O estilo grava nos canais 1–8 e o CASM redireciona para 9–16. Tocar no
   canal de origem em vez do de destino = instrumento errado / silêncio.
2. **NTR + NTT** definem a conversão de cada nota (ver
   `lib/src/style/chord_transposer.dart`).
3. **Note Low/High Limit**: a nota é dobrada por oitavas até cair na faixa.
4. **High Key**: com tônica acima do limite, o padrão cai uma oitava.
5. **Bypass + NTR ≠ ROOT TRANS** (ou bateria): a parte toca **como foi
   gravada**, sem transposição.

## Note Mute e Chord Mute — máscaras de bits (e os padrões alternativos)

Esses dois campos **são** máscaras de bits simples, e são a chave para os
estilos "estendidos" (a maioria dos estilos de fábrica modernos):

- `noteMute` (bytes 11–12): 12 bits, **bit n = tônica n toca** (C=0 … B=11).
- `chordMute` (bytes 13–17): 34 bits, **bit n = chord type n toca**
  (0=Maj … 33=sus2); o **bit 34** é o *autostart*.

Um estilo "estendido" grava **vários padrões para o mesmo canal de destino** —
por exemplo um em **maior** e outro em **menor** — e usa essas máscaras para
dizer qual toca. Exemplo real (arquivo `2.sty`, seções *Intro B/C*, *Ending B/C*):

| Canal | Nome | srcType | chordMute | Toca para |
|---|---|---|---|---|
| src#1 | Bass | 8 (m) | `00007FF00` | só os **11 tipos menores** |
| src#10 | Bass | 0 (Maj) | `3FFF800FF` | todos os **outros 23 tipos** |

`0x00007FF00` cobre exatamente m, m6, m7, m7b5, m(9), m9, m7(11), mM7, mM7(9),
dim e dim7; `0x3FFF800FF` é o complemento bit a bit. Os dois padrões **nunca**
tocam juntos.

> **Consequência de ignorar essas máscaras:** com um acorde menor, o app tocaria
> o padrão de maior *e* o de menor ao mesmo tempo, no mesmo canal — duas linhas
> de baixo, dois pianos, dobraduras desafinadas. É exatamente o sintoma de
> "estilo soando completamente errado". Por isso o player aplica as máscaras.

Valores `0` e "todos os bits ligados" (`0x0FFF` / `0x3FFFFFFFF`) significam "sem
restrição".

### Quando o acorde detectado não é coberto

O detector de acordes do app pode produzir um tipo que o estilo não previu
(ex.: detectou `C9` e o estilo só tem padrões para Maj/m/M7/m7). Nesse caso:

1. Se alguma regra do segmento cobre o acorde → o padrão que não serve
   simplesmente não toca (o alternativo toca no lugar). Comportamento normal.
2. Se **nenhuma** cobre → o player escolhe a regra do mesmo canal de destino que
   cobre **mais tipos da mesma família** (maior ou menor) e toca ignorando a
   máscara. Assim não silencia nada e nem toca vários padrões dobrados.

## Canais não mapeados

Um `CSEG` **não precisa** ter regra para todos os 16 canais — e muitas vezes não
tem, de propósito. No *West Coast Pop*, por exemplo, o `CSEG` de `Ending B` cobre
12 canais e **não** tem regra para os canais 4 e 13 (as cordas): a seção é mais
enxuta.

Somente os canais mapeados tocam (é o comportamento do teclado real). Se um canal
sem regra fosse tocando com uma regra inventada, a parte ia parar no canal
errado — no caso acima, o canal 4 cairia no 12, que é a guitarra, e a **corda
soaria como guitarra**.

Regra do player:
- estilo **com** CASM → canal sem regra **não toca**;
- estilo **sem** CASM algum → todos os canais tocam com a regra padrão
  (9/10 = ritmo, 11–13 = harmonia, resto melodia).

## Referências

- Jørgen Sørensen, *Yamaha Keyboard — CASM Section Format 1*
  <http://www.jososoft.dk/yamaha/articles/casm_1.htm>
- Wierzba & Bedesem, *Style Files – Introduction and Details* v2.1
- `docs/sty_analyzer.py` — decodificador de referência (Python)
