//===- FuzzerMain.cpp - main() function and flags -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// main() and flags.
//===----------------------------------------------------------------------===//

#include "FuzzerInternal.h"

#include <climits>
#include <cstring>
#include <unistd.h>
#include <iostream>

// ASAN options:
//   * don't dump the coverage to disk.
//   * enable coverage by default.
extern "C" const char *__asan_default_options() {
  return "coverage_pcs=0:coverage=1";
}

// Program arguments.
struct FlagDescription {
  const char *Name;
  const char *Description;
  int   Default;
  int   *Flag;
};

struct {
#define FUZZER_FLAG(Type, Name, Default, Description) Type Name;
#include "FuzzerFlags.def"
#undef FUZZER_FLAG
} Flags;

static FlagDescription FlagDescriptions [] {
#define FUZZER_FLAG(Type, Name, Default, Description) {#Name, Description, Default, &Flags.Name},
#include "FuzzerFlags.def"
#undef FUZZER_FLAG
};

static const size_t kNumFlags =
    sizeof(FlagDescriptions) / sizeof(FlagDescriptions[0]);

static std::vector<std::string> inputs;
static const char *ProgName;

static void PrintHelp() {
  std::cerr << "Usage: " << ProgName
            << " [-flag1=val1 [-flag2=val2 ...] ] [dir1 [dir2 ...] ]\n";
  std::cerr << "\nFlags: (strictly in form -flag=value)\n";
  size_t MaxFlagLen = 0;
  for (size_t F = 0; F < kNumFlags; F++)
    MaxFlagLen = std::max(strlen(FlagDescriptions[F].Name), MaxFlagLen);

  for (size_t F = 0; F < kNumFlags; F++) {
    const auto &D = FlagDescriptions[F];
    std::cerr << "  " << D.Name;
    for (size_t i = 0, n = MaxFlagLen - strlen(D.Name); i < n; i++)
      std::cerr << " ";
    std::cerr << "\t";
    std::cerr << D.Default << "\t" << D.Description << "\n";
  }
}

static const char *FlagValue(const char *Param, const char *Name) {
  size_t Len = strlen(Name);
  if (Param[0] == '-' && strstr(Param + 1, Name) == Param + 1 &&
      Param[Len + 1] == '=')
      return &Param[Len + 2];
  return nullptr;
}

static bool ParseOneFlag(const char *Param) {
  if (Param[0] != '-') return false;
  for (size_t F = 0; F < kNumFlags; F++) {
    const char *Name = FlagDescriptions[F].Name;
    const char *Str = FlagValue(Param, Name);
    if (Str)  {
      int Val = std::stol(Str);
      *FlagDescriptions[F].Flag = Val;
      if (Flags.verbosity >= 2)
        std::cerr << "Flag: " << Name << " " << Val << "\n";
      return true;
    }
  }
  PrintHelp();
  exit(1);
}

// We don't use any library to minimize dependencies.
static void ParseFlags(int argc, char **argv) {
  for (size_t F = 0; F < kNumFlags; F++)
    *FlagDescriptions[F].Flag = FlagDescriptions[F].Default;
  for (int A = 1; A < argc; A++) {
    if (ParseOneFlag(argv[A])) continue;
    inputs.push_back(argv[A]);
  }
}

int main(int argc, char **argv) {
  using namespace fuzzer;

  ProgName = argv[0];
  ParseFlags(argc, argv);
  if (Flags.help) {
    PrintHelp();
    return 0;
  }
  Fuzzer::FuzzingOptions Options;
  Options.Verbosity = Flags.verbosity;
  Options.MaxLen = Flags.max_len;
  Options.DoCrossOver = Flags.cross_over;
  Options.MutateDepth = Flags.mutate_depth;
  Options.ExitOnFirst = Flags.exit_on_first;
  Options.UseFullCoverageSet = Flags.use_full_coverage_set;
  if (!inputs.empty())
    Options.OutputCorpus = inputs[0];
  Fuzzer F(Options);

  unsigned seed = Flags.seed;
  // Initialize seed.
  if (seed == 0)
    seed = time(0) * 10000 + getpid();
  if (Flags.verbosity)
    std::cerr << "Seed: " << seed << "\n";
  srand(seed);

  // Timer
  if (Flags.timeout > 0)
    SetTimer(Flags.timeout);

  for (auto &inp : inputs)
    F.ReadDir(inp);

  if (F.CorpusSize() == 0)
    F.AddToCorpus(Unit());  // Can't fuzz empty corpus, so add an empty input.
  F.ShuffleAndMinimize();
  if (Flags.save_minimized_corpus)
    F.SaveCorpus();
  F.Loop(Flags.iterations < 0 ? INT_MAX : Flags.iterations);
  if (Flags.verbosity)
    std::cerr << "Done\n";
  return 1;
}
