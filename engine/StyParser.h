// Leitor de arquivos de estilo Yamaha (.sty / SFF1 / SFF2).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "StyleModel.h"

namespace yamaha {

class StyParser {
 public:
  /// Faz o parse de [bytes]. Retorna false em erro de formato.
  static bool parse(const std::vector<uint8_t>& bytes, YamahaStyle& out,
                    const std::string& fileName = "style.sty");

  /// Le um arquivo do disco e faz o parse.
  static bool parseFile(const std::string& path, YamahaStyle& out);
};

} // namespace yamaha
