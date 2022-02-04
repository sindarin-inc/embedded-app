#pragma once

#include "Displays/EinkET013TT1.hpp"

const EinkET013TT1Waveform BC_EINK_31_35C_WAVEFORM_FULL = {
    "BC edits of Eink 31-35C (FULL)",
    18,
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM>{
        Frame{bb : WHITE, bw : NOCHG, wb : NOCHG, ww : BLACK},
        Frame{bb : WHITE, bw : NOCHG, wb : NOCHG, ww : BLACK},
        Frame{bb : WHITE, bw : NOCHG, wb : NOCHG, ww : BLACK},
        Frame{bb : WHITE, bw : NOCHG, wb : NOCHG, ww : BLACK},
        Frame{bb : WHITE, bw : NOCHG, wb : NOCHG, ww : NOCHG},
        Frame{bb : WHITE, bw : NOCHG, wb : WHITE, ww : NOCHG},
        Frame{bb : WHITE, bw : BLACK, wb : WHITE, ww : NOCHG},
        Frame{bb : WHITE, bw : BLACK, wb : WHITE, ww : NOCHG},
        Frame{bb : WHITE, bw : BLACK, wb : WHITE, ww : NOCHG},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : NOCHG},
        Frame{bb : NOCHG, bw : WHITE, wb : BLACK, ww : NOCHG},
        Frame{bb : NOCHG, bw : NOCHG, wb : NOCHG, ww : NOCHG},
    },
};

const EinkET013TT1Waveform BC_EINK_31_35C_WAVEFORM_PARTIAL = {
    "BC edits of Eink 31-35C (PARTIAL)",
    13,
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM>{
        Frame{bb : NOCHG, bw : BLACK, wb : NOCHG, ww : NOCHG},
        Frame{bb : NOCHG, bw : BLACK, wb : NOCHG, ww : NOCHG},
        Frame{bb : NOCHG, bw : WHITE, wb : NOCHG, ww : WHITE},
        Frame{bb : NOCHG, bw : WHITE, wb : NOCHG, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : NOCHG, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : NOCHG, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : NOCHG, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : NOCHG, bw : NOCHG, wb : BLACK, ww : NOCHG},
        Frame{bb : NOCHG, bw : NOCHG, wb : NOCHG, ww : NOCHG},
    },
};

//
// The concept here is that when we are in inverted mode,
// we want to push everything black for the first few frames.
// Then we want to pull up the white bits extra hard.
//
// Do the opposite for regular mode.
//
const EinkET013TT1Waveform BC_INVERTED_EXPERIMENTAL_WAVEFORM_PARTIAL = {
    "BC EXPERIMENTAL INVERTED (PARTIAL)",
    13,
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM>{
        Frame{bb : BLACK, bw : BLACK, wb : BLACK, ww : BLACK},
        Frame{bb : BLACK, bw : BLACK, wb : BLACK, ww : BLACK},
        Frame{bb : NOCHG, bw : WHITE, wb : NOCHG, ww : NOCHG},
        Frame{bb : NOCHG, bw : WHITE, wb : NOCHG, ww : NOCHG},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : NOCHG},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : NOCHG, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : NOCHG, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : NOCHG, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : NOCHG, bw : WHITE, wb : BLACK, ww : NOCHG},
        Frame{bb : NOCHG, bw : NOCHG, wb : NOCHG, ww : NOCHG},
    },
};

const EinkET013TT1Waveform BC_REGULAR_EXPERIMENTAL_WAVEFORM_PARTIAL = {
    "BC EXPERIMENTAL REGULAR (PARTIAL)",
    13,
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM>{
        Frame{bb : WHITE, bw : WHITE, wb : WHITE, ww : WHITE},
        Frame{bb : WHITE, bw : WHITE, wb : WHITE, ww : WHITE},
        Frame{bb : NOCHG, bw : NOCHG, wb : BLACK, ww : NOCHG},
        Frame{bb : NOCHG, bw : NOCHG, wb : BLACK, ww : NOCHG},
        Frame{bb : NOCHG, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : WHITE},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : NOCHG},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : NOCHG},
        Frame{bb : BLACK, bw : WHITE, wb : BLACK, ww : NOCHG},
        Frame{bb : NOCHG, bw : WHITE, wb : BLACK, ww : NOCHG},
        Frame{bb : NOCHG, bw : NOCHG, wb : NOCHG, ww : NOCHG},
    },
};