
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2005-2016 Brian Roark and Google, Inc.
// Makes a normalized n-gram model from an input FST with raw counts.

#include <vector>

#include <fst/map.h>
#include <ngram/hist-arc.h>
#include <ngram/ngram-absolute.h>
#include <ngram/ngram-count-of-counts.h>
#include <ngram/ngram-katz.h>
#include <ngram/ngram-kneser-ney.h>
#include <ngram/ngram-make.h>
#include <ngram/ngram-unsmoothed.h>
#include <ngram/ngram-witten-bell.h>

DEFINE_double(witten_bell_k, 1, "Witten-Bell hyperparameter K");
DEFINE_double(discount_D, -1, "Absolute discount value D to use");
DEFINE_string(method, "katz",
              "One of: \"absolute\", \"katz\", \"kneser_ney\", "
              "\"presmoothed\", \"unsmoothed\", \"katz_frac\", "
              "\"witten_bell\" (default)");
DEFINE_bool(backoff, false, "Use backoff smoothing (default: method dependent");
DEFINE_bool(interpolate, false,
            "Use interpolated smoothing (default: method dependent)");
DEFINE_int64(bins, -1, "Number of bins for katz or absolute discounting");
DEFINE_int64(backoff_label, 0, "Backoff label");
DEFINE_double(norm_eps, ngram::kNormEps, "Normalization check epsilon");
DEFINE_bool(check_consistency, false, "Check model consistency");
DEFINE_string(count_of_counts, "", "Read count-of-counts from file");

namespace fst {

// Mapper (from HistogramArc to StdArc)
struct ToStdArcMapper {
  typedef ngram::HistogramArc FromArc;
  typedef StdArc ToArc;

  ToArc operator()(const FromArc &arc) const {
    return ToArc(arc.ilabel, arc.olabel, arc.weight.Value(0).Value(),
                 arc.nextstate);
  }

  MapFinalAction FinalAction() const { return MAP_NO_SUPERFINAL; }
  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }
  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS; }
  uint64 Properties(uint64 props) const { return props; }
};

}  // namespace fst

int main(int argc, char **argv) {
  string usage = "Make ngram model from input count file.\n\n  Usage: ";
  usage += argv[0];
  usage += " [--options] [in.fst [out.fst]]\n";
  std::set_new_handler(FailedNewHandler);
  SET_FLAGS(usage.c_str(), &argc, &argv, true);

  if (argc < 1 || argc > 3) {
    ShowUsage();
    return 1;
  }
  bool prefix_norm = false;
  if (FLAGS_method == "presmoothed") {  // only for use with randgen counts
    prefix_norm = true;
    FLAGS_method = "unsmoothed";  // normalizes only based on prefix count
  }
  if (FLAGS_backoff && FLAGS_interpolate) {
    // Fails if both backoff and interpolate are chosen.
    LOG(ERROR) << argv[0]
               << ": Model can either be backoff or interpolated, not both";
    return 1;
  }

  string in_name = (argc > 1 && (strcmp(argv[1], "-") != 0)) ? argv[1] : "";
  string out_name = (argc > 2 && (strcmp(argv[2], "-") != 0)) ? argv[2] : "";

  fst::StdFst *ccfst = 0;
  if (!FLAGS_count_of_counts.empty()) {
    ccfst = fst::StdFst::Read(FLAGS_count_of_counts);
    if (!ccfst) return 1;
  }

  if (FLAGS_method == "kneser_ney") {
    fst::StdVectorFst *fst = fst::StdVectorFst::Read(in_name);
    if (!fst) return 1;
    ngram::NGramKneserNey ngram(fst, FLAGS_backoff, FLAGS_backoff_label,
                                FLAGS_norm_eps, FLAGS_check_consistency,
                                FLAGS_discount_D, FLAGS_bins);
    if (ccfst) ngram.SetCountOfCounts(*ccfst);
    if (!ngram.MakeNGramModel()) {
      NGRAMERROR() << "NGramKneserNey: failed to make model";
      return 1;
    }
    ngram.GetFst().Write(out_name);
  } else if (FLAGS_method == "absolute") {
    fst::StdVectorFst *fst = fst::StdVectorFst::Read(in_name);
    if (!fst) return 1;
    ngram::NGramAbsolute ngram(fst, FLAGS_backoff, FLAGS_backoff_label,
                               FLAGS_norm_eps, FLAGS_check_consistency,
                               FLAGS_discount_D, FLAGS_bins);
    if (ccfst) ngram.SetCountOfCounts(*ccfst);
    if (!ngram.MakeNGramModel()) {
      NGRAMERROR() << "NGramAbsolute: failed to make model";
      return 1;
    }
    ngram.GetFst().Write(out_name);
  } else if (FLAGS_method == "katz") {
    fst::StdVectorFst *fst = fst::StdVectorFst::Read(in_name);
    if (!fst) return 1;
    ngram::NGramKatz<fst::StdArc> ngram(
        fst, !FLAGS_interpolate, FLAGS_backoff_label, FLAGS_norm_eps,
        FLAGS_check_consistency, FLAGS_bins);
    if (ccfst) ngram.SetCountOfCounts(*ccfst);
    if (!ngram.MakeNGramModel()) {
      NGRAMERROR() << "NGramKatz: failed to make model";
      return 1;
    }
    ngram.GetFst().Write(out_name);
  } else if (FLAGS_method == "katz_frac") {
    fst::VectorFst<ngram::HistogramArc> *fst =
        fst::VectorFst<ngram::HistogramArc>::Read(in_name);
    if (!fst) return 1;
    ngram::NGramKatz<ngram::HistogramArc> ngram(
        fst, !FLAGS_interpolate, FLAGS_backoff_label, FLAGS_norm_eps,
        FLAGS_check_consistency, FLAGS_bins);
    if (ccfst) ngram.SetCountOfCounts(*ccfst);
    if (!ngram.MakeNGramModel()) {
      NGRAMERROR() << "NGramKatz(Frac): failed to make model";
      return 1;
    }
    fst::StdVectorFst out_fst;
    Map(ngram.GetFst(), &out_fst, fst::ToStdArcMapper());
    out_fst.Write(out_name);
  } else if (FLAGS_method == "witten_bell") {
    fst::StdVectorFst *fst = fst::StdVectorFst::Read(in_name);
    if (!fst) return 1;
    ngram::NGramWittenBell ngram(fst, FLAGS_backoff, FLAGS_backoff_label,
                                 FLAGS_norm_eps, FLAGS_check_consistency,
                                 FLAGS_witten_bell_k);
    if (!ngram.MakeNGramModel()) {
      NGRAMERROR() << "NGramWittenBell: failed to make model";
      return 1;
    }
    ngram.GetFst().Write(out_name);
  } else if (FLAGS_method == "unsmoothed") {
    fst::StdVectorFst *fst = fst::StdVectorFst::Read(in_name);
    if (!fst) return 1;
    ngram::NGramUnsmoothed ngram(fst, !FLAGS_interpolate, prefix_norm,
                                 FLAGS_backoff_label, FLAGS_norm_eps,
                                 FLAGS_check_consistency);
    if (!ngram.MakeNGramModel()) {
      NGRAMERROR() << "NGramUnsmoothed: failed to make model";
      return 1;
    }
    ngram.GetFst().Write(out_name);
  } else {
    LOG(ERROR) << argv[0] << ": bad model method: " << FLAGS_method;
    return 1;
  }
  return 0;
}
