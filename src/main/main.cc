// Copyright 2023 Justin Hu
//
// This file is part of SIDLE.
//
// SIDLE is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// SIDLE is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along
// with SIDLE. If not, see <https://www.gnu.org/licenses/>.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stb_image_write.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <vector>

using namespace std;
using namespace std::filesystem;
using namespace nlohmann;

using colour = array<uint8_t, 4>;

struct Resolution {
  int width;
  int height;
};

uint8_t parseHexit(char c) {
  if ('0' <= c && c <= '9') {
    return c - '0';
  } else if ('a' <= c && c <= 'f') {
    return c - 'a' + 10;
  } else {
    // 'A' <= c && c <= 'F'
    return c - 'A' + 10;
  }
}

colour parseColour(string s) {
  if (s.front() == '#') {
    s = s.substr(1);
  }

  if (s.find_first_not_of("0123456789abcdefABCDEF") != string::npos) {
    throw runtime_error("invalid colour");
  }

  if (s.size() == 3) {
    return colour{
        static_cast<uint8_t>(parseHexit(s[0]) * 0x11),
        static_cast<uint8_t>(parseHexit(s[1]) * 0x11),
        static_cast<uint8_t>(parseHexit(s[2]) * 0x11),
        0xff,
    };
  } else if (s.size() == 4) {
    return colour{
        static_cast<uint8_t>(parseHexit(s[0]) * 0x11),
        static_cast<uint8_t>(parseHexit(s[1]) * 0x11),
        static_cast<uint8_t>(parseHexit(s[2]) * 0x11),
        static_cast<uint8_t>(parseHexit(s[3]) * 0x11),
    };
  } else if (s.size() == 6) {
    return colour{
        static_cast<uint8_t>(parseHexit(s[0]) * 0x10 + parseHexit(s[1])),
        static_cast<uint8_t>(parseHexit(s[2]) * 0x10 + parseHexit(s[3])),
        static_cast<uint8_t>(parseHexit(s[4]) * 0x10 + parseHexit(s[5])),
        0xff,
    };
  } else if (s.size() == 8) {
    return colour{
        static_cast<uint8_t>(parseHexit(s[0]) * 0x10 + parseHexit(s[1])),
        static_cast<uint8_t>(parseHexit(s[2]) * 0x10 + parseHexit(s[3])),
        static_cast<uint8_t>(parseHexit(s[4]) * 0x10 + parseHexit(s[5])),
        static_cast<uint8_t>(parseHexit(s[6]) * 0x10 + parseHexit(s[7])),
    };
  } else {
    throw runtime_error("invalid colour");
  }
}

int main(int argc, char **argv) {
  if (argc != 2) {
    cerr << "usage: " << argv[0] << " <description-file>\n";
    return 1;
  }

  ifstream fin = ifstream(argv[1]);
  if (!fin.good()) {
    cerr << "error: could not open " << argv[1] << "\n";
    return 1;
  }

  try {
    json file = json::parse(fin);

    path outputFolder =
        path(file.at("outputPath").get<string>()).lexically_normal();

    vector<Resolution> resolutions;
    for (auto const &[_, resolution] : file.at("resolutions").items()) {
      resolutions.emplace_back(Resolution{
          .width = resolution.at(0).get<int>(),
          .height = resolution.at(1).get<int>(),
      });
    }

    for (auto const &[_, image] : file.at("images").items()) {
      for (Resolution resolution : resolutions) {
        path resolutionFolder =
            outputFolder / ("res"s + to_string(resolution.width) + "x"s +
                            to_string(resolution.height));
        create_directories(resolutionFolder);
        if (status(resolutionFolder).type() != file_type::directory) {
          throw runtime_error("could not create output folder");
        }

        path outputPath =
            resolutionFolder / (image.at("name").get<string>() + ".tga"s);

        int width = round(image.at("width").get<double>() * resolution.width);
        int height =
            round(image.at("height").get<double>() * resolution.height);

        unique_ptr<uint8_t[]> output =
            make_unique<uint8_t[]>(width * height * 4);

        colour backgroundColour =
            parseColour(image.at("background").get<string>());
        for (size_t idx = 0; idx < width * height; ++idx) {
          copy(backgroundColour.cbegin(), backgroundColour.cend(),
               output.get() + (idx * 4));
        }

        for (auto const &[_, element] : image.at("elements").items()) {
          string elementType = element.at("type").get<string>();
          if (elementType == "rectangle") {
            colour rectangleColour =
                parseColour(element.at("colour").get<string>());

            size_t startX = round(element.at("x").get<double>() * width);
            size_t endX = min(
                static_cast<int>(
                    startX + round(element.at("width").get<double>() * width)),
                width);
            size_t startY = round(element.at("y").get<double>() * height);
            size_t endY =
                min(static_cast<int>(
                        startY +
                        round(element.at("height").get<double>() * height)),
                    height);
            for (size_t y = startY; y < endY; ++y) {
              for (size_t x = startX; x < endX; ++x) {
                copy(rectangleColour.cbegin(), rectangleColour.cend(),
                     output.get() + ((y * width + x) * 4));
              }
            }
          } else {
            throw runtime_error("invalid type");
          }
        }

        stbi_write_tga(outputPath.c_str(), width, height, 4, output.get());
      }
    }
  } catch (json::parse_error const &e) {
    cerr << "error: could not parse " << argv[1] << "\n";
    return 1;
  } catch (json::exception const &e) {
    cerr << "error: invalid description\n" << e.what() << "\n";
  } catch (runtime_error const &e) {
    cerr << "error: " << e.what() << "\n";
  }

  return 0;
}
