#pragma once

#include "Displays/EinkET013TT1.hpp"

const EinkET013TT1Waveform BC_EINK_31_35C_WAVEFORM_FULL = {
    "BC edits of Eink 31-35C (FULL)",
    17,
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