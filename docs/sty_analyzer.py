#!/usr/bin/env python3
"""Reference parser for Yamaha .sty (Style File Format SFF1/SFF2).

Used to validate the parsing logic before porting to Dart.
Parses: SMF chunks, section markers (Main A..D, Fill In, Intro, Ending),
and the CASM block (CSEG / Ctab / Cntt) that drives chord retrigger rules.
"""
import struct
import sys


def read_vlq(data, i):
    """Read MIDI variable-length quantity."""
    value = 0
    while True:
        b = data[i]
        i += 1
        value = (value << 7) | (b & 0x7F)
        if not (b & 0x80):
            break
    return value, i


def parse_smf(data):
    assert data[0:4] == b'MThd', "not a MIDI file"
    hlen = struct.unpack('>I', data[4:8])[0]
    fmt, ntrk, div = struct.unpack('>HHH', data[8:8 + hlen])
    print(f"SMF format={fmt} tracks={ntrk} division={div} ppq")
    i = 8 + hlen
    tracks = []
    while i < len(data) and data[i:i + 4] == b'MTrk':
        tlen = struct.unpack('>I', data[i + 4:i + 8])[0]
        start = i + 8
        end = start + tlen
        tracks.append((start, end))
        i = end
    return fmt, div, tracks, i  # i points to after last MTrk -> CASM region


def parse_track_meta(data, start, end):
    """Walk track events to collect markers, fn texts, and SFF meta."""
    i = start
    running = None
    sections = []  # (tick, marker_text)
    tick = 0
    while i < end:
        dt, i = read_vlq(data, i)
        tick += dt
        status = data[i]
        if status == 0xFF:  # meta
            mtype = data[i + 1]
            length, j = read_vlq(data, i + 2)
            payload = data[j:j + length]
            i = j + length
            if mtype == 0x06:  # marker
                sections.append((tick, payload.decode('latin-1')))
        elif status == 0xF0 or status == 0xF7:  # sysex
            i += 1
            length, j = read_vlq(data, i)
            i = j + length
        else:
            if status & 0x80:
                running = status
                i += 1
            else:
                status = running
            ev = status & 0xF0
            if ev in (0xC0, 0xD0):
                i += 1
            else:
                i += 2
    return sections


def parse_casm(data, i):
    """Parse the CASM block."""
    if data[i:i + 4] != b'CASM':
        # search
        idx = data.find(b'CASM', i)
        if idx < 0:
            print("No CASM block found")
            return
        i = idx
    casm_len = struct.unpack('>I', data[i + 4:i + 8])[0]
    print(f"\nCASM block at {i}, length={casm_len}")
    j = i + 8
    end = j + casm_len
    while j < end - 8:
        if data[j:j + 4] == b'CSEG':
            seg_len = struct.unpack('>I', data[j + 4:j + 8])[0]
            seg_start = j + 8
            seg_end = seg_start + seg_len
            # Sdec name list
            k = seg_start
            if data[k:k + 4] == b'Sdec':
                sdec_len = struct.unpack('>I', data[k + 4:k + 8])[0]
                names = data[k + 8:k + 8 + sdec_len].decode('latin-1')
                print(f"  CSEG -> sections: [{names}]")
                k = k + 8 + sdec_len
            # Ctab entries
            while k < seg_end - 8:
                if data[k:k + 4] in (b'Ctab', b'Ctb2'):
                    tag = data[k:k + 4].decode()
                    ctl = struct.unpack('>I', data[k + 4:k + 8])[0]
                    cdata = data[k + 8:k + 8 + ctl]
                    parse_ctab(tag, cdata)
                    k = k + 8 + ctl
                else:
                    k += 1
            j = seg_end
        else:
            j += 1


# Chord type ids do CASM (0..33), na ordem usada pelo chord mute.
TYPE_NAMES = ["Maj", "6", "M7", "M7#11", "add9", "M9", "6/9", "aug", "m", "m6",
              "m7", "m7b5", "m(9)", "m9", "m7(11)", "mM7", "mM7(9)", "dim",
              "dim7", "7", "7sus4", "7b5", "9", "7#11", "13", "7b9", "7b13",
              "7#9", "M7aug", "7aug", "1+8", "1+5", "sus4", "sus2"]

def parse_ctab(tag, d):
    """Decode one Ctab/Ctb2 entry (per-source-channel chord rules).

    Layout do Ctab (SFF1, 27 bytes) — ver casm_layout.md:
      0      source channel (0-based)
      1..8   voice name (8 chars)
      9      destination channel (0-based, 8..15)
      10     editable flag
      11..12 note mute (codificado em nibbles estilo Gray)
      13..17 chord mute (idem; bit 34 = autostart)
      18     source root (0..11)
      19     source chord type (0..33)
      20     NTR  (0=ROOT TRANS, 1=ROOT FIXED, 2=Guitar)
      21     NTT  (0=Bypass, 1=Melody, 2=Chord, 3=Bass, 4=Melodic Min, 5=Harmonic Min)
      22     high key (0..11)
      23     note low limit
      24     note high limit
      25     RTR (retrigger rule)
      26     end marker

    Ctb2 (SFF2) tem os mesmos 22 primeiros bytes e depois mid_lo, mid_hi e
    TRÊS zonas de 6 bytes: [NTR, NTT, high key, note low, note high, RTR].
    """
    if len(d) < 26:
        print(f"    [{tag}] registro curto ({len(d)} bytes)")
        return
    src = d[0] & 0x0F
    name = d[1:9].decode('latin-1').rstrip()
    dest = d[9] & 0x0F
    if not (8 <= dest <= 15):
        dest = src | 0x08
    note_mute = ((d[11] << 8) | d[12]) & 0x0FFF
    cm = 0
    for x in d[13:18]:
        cm = (cm << 8) | x
    autostart = bool(cm & (1 << 34))
    cm &= (1 << 34) - 1
    src_root = d[18] % 12
    src_type = d[19] if d[19] < 34 else 2
    ntr = {0: 'ROOT TRANS', 1: 'ROOT FIXED', 2: 'Guitar'}.get(d[20], 'ROOT TRANS')
    rtr = {0: 'Stop', 1: 'PitchShift', 2: 'PitchShiftRoot', 3: 'Retrigger',
           4: 'RetriggerRoot', 5: 'NoteGen'}.get(d[25], 'NoteGen')

    def ntt_name(v):
        ntt_old = {0: 'Bypass', 1: 'Melody', 2: 'Chord', 3: 'Bass',
                   4: 'MelodicMin', 5: 'HarmonicMin'}
        ntt_new = {6: 'HarmonicMin5', 7: 'NaturalMin', 8: 'NaturalMin5',
                   9: 'Dorian', 10: 'Dorian5'}
        name = ntt_old.get(v) or ntt_new.get(v) or 'Melody'
        bass = ' BassOn' if (v == 3 or v & 0x80) else ''
        return name + bass

    def zone_line(label, ntr_b, ntt_b, high_b, lo_b, hi_b, rtr_b):
        ntr_s = {0: 'RootTrans', 1: 'RootFixed', 2: 'Guitar'}.get(ntr_b, 'RootTrans')
        return (f"          {label}: NTR={ntr_s} NTT={ntt_name(ntt_b)} "
                f"highKey={high_b % 12} lim={lo_b & 0x7F}-{hi_b & 0x7F} "
                f"RTR={rtr_b}")

    print(f"    [{tag}] src#{src} '{name}' dst#{dest} srcRoot={src_root} "
          f"srcType={src_type} noteMute={note_mute:03X} chordMute={cm:09X}"
          f"{' autoStart' if autostart else ''}")
    # Quais acordes este padrão toca (máscara de 34 bits: bit n = chord type n).
    on = [i for i in range(34) if cm & (1 << i)]
    if len(on) == 34:
        print("          toca com: TODOS os tipos de acorde")
    elif not on:
        print("          toca com: NENHUM tipo (padrao nunca usado)")
    else:
        tipos = ", ".join(f"{i}:{TYPE_NAMES[i] or 'Maj'}" for i in on)
        print(f"          toca com ({len(on)}/34): {tipos}")
        off = [i for i in range(34) if not (cm & (1 << i))]
        print(f"          mudo para: {', '.join(f'{i}:{TYPE_NAMES[i] or chr(77)+chr(97)+chr(106)}' for i in off)}")
    if tag == 'Ctb2' and len(d) >= 40:
        # SFF2: tres zonas de 6 bytes a partir do offset 22.
        for i, base in enumerate((22, 28, 34)):
            z = d[base:base + 6]
            rtrz = {0: 'Stop', 1: 'PitchShift', 2: 'PitchShiftRoot',
                    3: 'Retrigger', 4: 'RetriggerRoot', 5: 'NoteGen'}.get(z[5], 'NoteGen')
            print(zone_line(f"zona{i}", z[0], z[1], z[2], z[3], z[4], rtrz))
    else:
        # SFF1: NTR=20, NTT=21, highKey=22, low=23, high=24, RTR=25.
        print(zone_line("zona0", d[20], d[21], d[22], d[23], d[24], rtr))


def main():
    path = sys.argv[1]
    with open(path, 'rb') as f:
        data = f.read()
    fmt, div, tracks, after = parse_smf(data)
    for (s, e) in tracks:
        sections = parse_track_meta(data, s, e)
        print(f"\nMarkers ({len(sections)}):")
        for t, m in sections:
            print(f"  tick {t:6d}: {m}")
    parse_casm(data, after)


if __name__ == '__main__':
    main()
