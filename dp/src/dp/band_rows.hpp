#pragma once
// The recorded band's file (dp_band.txt), written by the mod (repair.hpp, from the anchor rows) and
// read by --bandtrack (bands.hpp loadBandTrack). Kept here, free of the game, so the writer and the
// reader can be run against each other in dptest.
//
// Rows open only on a change: `t,I,floor,ceil,src` (or `t,D,v,v,src` for a collapsed band) when the
// kind, the floor or the ceiling changes, and `t,S,src` when only the SOURCE does (1 = the camera's
// band, 0 = the portal's: [layer+0x23c] != 0 && [layer+0x2a0] == 0). The values are written with the
// stream's own float format, which is the format the file has always had. A reader that knows only
// four (or five) columns does not take an S row for a band: its kind is none of I/D/N/U and it is
// not three numbers, so it is counted as unreadable and skipped, and every row it did read is the
// row it always read.
#include <sstream>
#include <string>
#include <vector>

namespace dp {

// One tick of the anchor rows: `written` is whether the writer looks at it at all (the tick, the one
// before or the one after is in a band mode for either body); a tick with no anchor row is not given.
struct BandSample {
    long long t = 0;
    bool written = false;
    float pmin = 0.f, pmax = 0.f;
    int cam = -1;
};

inline std::string bandRows(const std::vector<BandSample>& samples, long long* rowsOut = nullptr) {
    std::ostringstream o;
    float lf = -1e9f, lc = -1e9f;
    char lk = 0;
    int ls = -2;
    long long rows = 0;
    for (const BandSample& s : samples) {
        if (!s.written) continue;
        const char k = (s.pmax > s.pmin) ? 'I' : 'D';
        if (k == lk && s.pmin == lf && s.pmax == lc) {
            if (s.cam != ls) {
                o << s.t << ",S," << s.cam << '\n';
                ls = s.cam;
                ++rows;
            }
            continue;
        }
        lk = k; lf = s.pmin; lc = s.pmax; ls = s.cam;
        if (k == 'I')
            o << s.t << ",I," << s.pmin << ',' << s.pmax << ',' << s.cam << '\n';
        else
            o << s.t << ",D," << s.pmin << ',' << s.pmin << ',' << s.cam << '\n';
        ++rows;
    }
    if (rowsOut) *rowsOut = rows;
    return o.str();
}

}  // namespace dp
