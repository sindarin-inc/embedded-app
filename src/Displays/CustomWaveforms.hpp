#pragma once

#include "Displays/EinkWaveform.hpp"

// Pushes black a little more at the end to reduce ghosting
const EinkET013TT1Waveform BC_REGULAR_EINK_10_13C_WAVEFORM_AU4 = {
    "Eink 10-13C (AU4)",
    4,
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM>{
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = NOCHG, .wb = NOCHG, .ww = NOCHG},
    },
};

// Pushes white a little more at the end to reduce ghosting
const EinkET013TT1Waveform BC_INVERTED_EINK_10_13C_WAVEFORM_AU4 = {
    "Eink 10-13C (AU4)",
    4,
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM>{
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = NOCHG, .wb = NOCHG, .ww = NOCHG},
    },
};

const EinkET013TT1Waveform BC_EINK_31_35C_WAVEFORM_FULL = {
    "BC edits of Eink 31-35C (FULL)",
    18,
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM>{
        Frame{.bb = WHITE, .bw = NOCHG, .wb = NOCHG, .ww = BLACK},
        Frame{.bb = WHITE, .bw = NOCHG, .wb = NOCHG, .ww = BLACK},
        Frame{.bb = WHITE, .bw = NOCHG, .wb = NOCHG, .ww = BLACK},
        Frame{.bb = WHITE, .bw = NOCHG, .wb = NOCHG, .ww = BLACK},
        Frame{.bb = WHITE, .bw = NOCHG, .wb = NOCHG, .ww = NOCHG},
        Frame{.bb = WHITE, .bw = NOCHG, .wb = WHITE, .ww = NOCHG},
        Frame{.bb = WHITE, .bw = BLACK, .wb = WHITE, .ww = NOCHG},
        Frame{.bb = WHITE, .bw = BLACK, .wb = WHITE, .ww = NOCHG},
        Frame{.bb = WHITE, .bw = BLACK, .wb = WHITE, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = NOCHG, .wb = NOCHG, .ww = NOCHG},
    },
};

const EinkET013TT1Waveform BC_EINK_31_35C_WAVEFORM_PARTIAL = {
    "BC edits of Eink 31-35C (PARTIAL)",
    13,
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM>{
        Frame{.bb = NOCHG, .bw = BLACK, .wb = NOCHG, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = BLACK, .wb = NOCHG, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = NOCHG, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = NOCHG, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = NOCHG, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = NOCHG, .wb = NOCHG, .ww = NOCHG},
    },
};

//
// There seem to be three main moves required to make partial refresh
// look good on the Eink ET013TT1 display. They are dependent on the
// background color so we need two different waveforms depending on what
// mode we are in. The three moves are the same for each:
//
// (1) For 2 or 3 frames, reinforce the background color for the pixels
// that remain in the background while flipping the pixels that are
// meant to be changed. Don't reinforce the pixels that are meant to
// stay in the foreground or there will be spots that seem "darker"
// in the foreground drawing.
//
// (2) For 2 or 3 frames, continue to reinforce the background color
// for background pixels that remain background pixels, but
// start to reinforce unchanged foreground pixels. Keep hammering
// the changed pixels.
//
// (3) For 4-6 frames (2x each of the previous frame counts),
// stop reinforcing the background pixels that remain unchanged
// and stop hammering the pixels that change from foreground to
// background. Essentially, stop drawing the background pixels
// and only draw the foreground pixels (both ones that changed to
// foreground and ones that remain in the foreground).
//
// My hypothesis for why this is necessary is that there is some bleed
// bewteen pixels. So if you pull BG and FG at the same time for all
// frames the BG essentailly messes with the FG. Of course, you do
// need to pull BG at least some. NOCHG doesn't seem to work like
// the simple PARTIAL waveform from Eink implies. This is also due
// to the bleed I suspect.
//
// Finally, in my experiments, pulling black down to white is a lot
// "easier" from a frame perspective than pulling white to black.
// So in the partial refresh mode for inverted, it takes 3 frames
// each of the first two moves and 6 frames of the second to yield
// something visually similar to 2, 2, 4 in regular mode. This
// probably should have some more experimentation to it.
//
// Assume VCOM for inverted to be ~-1500 and VCOM for regular to be ~-4000.
//
// I'd like to keep the below lines commented as a reminder that we may
// need to do more frames if people complain about ghosting.
//
const EinkET013TT1Waveform BC_INVERTED_EXPERIMENTAL_WAVEFORM_PARTIAL = {
    "BC EXPERIMENTAL INVERTED (PARTIAL)",
    10,
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM>{
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = NOCHG, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = NOCHG, .ww = WHITE},
        //  Frame{.bb = NOCHG, .bw = WHITE, .wb = NOCHG, .ww = WHITE},
        //  Frame{.bb = NOCHG, .bw = WHITE, .wb = NOCHG, .ww = WHITE},
        //  Frame{.bb = NOCHG, .bw = WHITE, .wb = NOCHG, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = NOCHG, .wb = NOCHG, .ww = NOCHG},
    },
};

const EinkET013TT1Waveform BC_REGULAR_EXPERIMENTAL_WAVEFORM_PARTIAL = {
    "BC EXPERIMENTAL REGULAR (PARTIAL)",
    11,
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM>{
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = NOCHG, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = NOCHG, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = NOCHG, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = NOCHG, .wb = BLACK, .ww = NOCHG},
        // Frame{.bb = BLACK, .bw = NOCHG, .wb = BLACK, .ww = NOCHG},
        // Frame{.bb = BLACK, .bw = NOCHG, .wb = BLACK, .ww = NOCHG},
        // Frame{.bb = BLACK, .bw = NOCHG, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = NOCHG, .wb = NOCHG, .ww = NOCHG},
    },
};

const EinkET013TT1Waveform BC_REGULAR_31_35C_WAVEFORM_FULL = {
    "BC REGULAR FULL 31-35C",
    28,
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM>{
        Frame{.bb = WHITE, .bw = NOCHG, .wb = NOCHG, .ww = BLACK},
        Frame{.bb = WHITE, .bw = BLACK, .wb = WHITE, .ww = BLACK},
        Frame{.bb = WHITE, .bw = BLACK, .wb = WHITE, .ww = BLACK},
        Frame{.bb = WHITE, .bw = BLACK, .wb = WHITE, .ww = BLACK},
        Frame{.bb = WHITE, .bw = BLACK, .wb = WHITE, .ww = NOCHG},
        Frame{.bb = WHITE, .bw = BLACK, .wb = WHITE, .ww = NOCHG},
        Frame{.bb = WHITE, .bw = BLACK, .wb = WHITE, .ww = NOCHG},
        Frame{.bb = WHITE, .bw = BLACK, .wb = WHITE, .ww = NOCHG},
        Frame{.bb = WHITE, .bw = BLACK, .wb = WHITE, .ww = NOCHG},
        Frame{.bb = WHITE, .bw = BLACK, .wb = WHITE, .ww = NOCHG},
        Frame{.bb = WHITE, .bw = BLACK, .wb = WHITE, .ww = NOCHG},
        Frame{.bb = WHITE, .bw = BLACK, .wb = WHITE, .ww = NOCHG},
        Frame{.bb = WHITE, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = NOCHG, .wb = NOCHG, .ww = NOCHG},
    },
};

const EinkET013TT1Waveform BC_REGULAR_AXIOM_WAVEFORM_PARTIAL = {
    "BC EXPERIMENTAL REGULAR (PARTIAL)",
    12,
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM>{
        Frame{.bb = NOCHG, .bw = BLACK, .wb = NOCHG, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = BLACK, .wb = WHITE, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = BLACK, .wb = WHITE, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = NOCHG, .ww = NOCHG},
        Frame{.bb = NOCHG, .bw = NOCHG, .wb = NOCHG, .ww = NOCHG},
    },
};

const EinkET013TT1Waveform BC_INVERTED_31_35C_WAVEFORM_FULL = {
    "BC INVERTED FULL 31-35C",
    22,
    std::array<Frame, MAX_FRAMES_PER_WAVEFORM>{
        Frame{.bb = WHITE, .bw = NOCHG, .wb = NOCHG, .ww = BLACK},
        Frame{.bb = WHITE, .bw = NOCHG, .wb = NOCHG, .ww = BLACK},
        Frame{.bb = WHITE, .bw = NOCHG, .wb = NOCHG, .ww = BLACK},
        Frame{.bb = WHITE, .bw = NOCHG, .wb = NOCHG, .ww = BLACK},
        Frame{.bb = NOCHG, .bw = NOCHG, .wb = NOCHG, .ww = BLACK},
        Frame{.bb = NOCHG, .bw = BLACK, .wb = NOCHG, .ww = BLACK},
        Frame{.bb = NOCHG, .bw = BLACK, .wb = WHITE, .ww = BLACK},
        Frame{.bb = NOCHG, .bw = BLACK, .wb = WHITE, .ww = BLACK},
        Frame{.bb = NOCHG, .bw = BLACK, .wb = WHITE, .ww = BLACK},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = NOCHG},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = BLACK, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = WHITE, .wb = BLACK, .ww = WHITE},
        Frame{.bb = NOCHG, .bw = NOCHG, .wb = NOCHG, .ww = NOCHG},
    },
};