#pragma once

#include "mesh.h"

#include <string>
#include <unordered_map>
#include <tuple>
#include <cstdint>

//CTR letters
//wumpa fruit
//wumpa crate, item crate
//time crate 123
//start flag

class SimpleLevelInstances
{
public:
  static constexpr uint16_t LETTER_C = 0;
  static constexpr uint16_t LETTER_T = 1;
  static constexpr uint16_t LETTER_R = 2;
  static constexpr uint16_t WUMPA_FRUIT = 3;
  static constexpr uint16_t WUMPA_CRATE = 4;
  static constexpr uint16_t ITEM_CRATE = 5;
  static constexpr uint16_t TIME_CRATE_1 = 6;
  static constexpr uint16_t TIME_CRATE_2 = 7;
  static constexpr uint16_t TIME_CRATE_3 = 8;
  static constexpr uint16_t START_FLAG = 9;

  static const std::unordered_map<std::string, uint16_t> mesh_prefixes_to_enum;

private:
  static std::unordered_map<uint16_t, Mesh> mesh_instances;
  static inline const std::unordered_map<uint16_t, std::tuple<std::string, std::string>> FILEPATHS = {
    {LETTER_C, std::make_tuple<std::string, std::string>("LetterC.obj", "t_letters.png")},
    {LETTER_T, std::make_tuple<std::string, std::string>("LetterT.obj", "t_letters.png")},
    {LETTER_R, std::make_tuple<std::string, std::string>("LetterR.obj", "t_letters.png")},
    {WUMPA_FRUIT, std::make_tuple<std::string, std::string>("Wumpafruit.obj", "t_fruit.png")},
    {WUMPA_CRATE, std::make_tuple<std::string, std::string>("Wumpacrate.obj", "t_crate.png")},
    {ITEM_CRATE, std::make_tuple<std::string, std::string>("Itemcrate.obj", "t_crate.png")},
    {TIME_CRATE_1, std::make_tuple<std::string, std::string>("Timecrate1.obj", "t_timecrate.png")},
    {TIME_CRATE_2, std::make_tuple<std::string, std::string>("Timecrate2.obj", "t_timecrate.png")},
    {TIME_CRATE_3, std::make_tuple<std::string, std::string>("Timecrate3.obj", "t_timecrate.png")},
    {START_FLAG, std::make_tuple<std::string, std::string>("Startflag.obj", "t_startflag.png")},
  };
public:

  static void GenerateRenderLevInstData();
  static const Mesh& GetMeshInstance(uint16_t instanceType);
};