// Aplica as regras Yamaha de conversão de acorde (NTR/NTT) as notas do estilo.
#pragma once

#include "Chord.h"
#include "StyleModel.h"

namespace yamaha {

class ChordTransposer {
 public:
  /// Converte [sourceNote] para o acorde [chord] segundo [rule].
  /// Retorna -1 quando a nota deve ser silenciada (note/chord mute do CASM).
  ///
  /// [ignoreMute] pula a verificacao de mute - usado quando nenhuma regra do
  /// segmento cobre o acorde detectado e o player escolheu tocar o padrao mais
  /// proximo de proposito.
  static int transposeNote(int sourceNote, const Chord& chord, const CtabRule& rule,
                           bool ignoreMute = false);

  /// Mapeia uma ChordQuality para o chord type id do CASM (0..33).
  static int typeId(ChordQuality q) { return yamahaTypeId(q); }
};

} // namespace yamaha
