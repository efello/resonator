// Dumps impulse response (fs given on cmdline) for a chosen setting. Python compares with the analog model.
#include "../Source/ResonatorDSP.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace resonator;
int main (int argc, char** argv)
{
    double fs = atof (argv[1]); int mode = atoi (argv[2]); double emph = atof (argv[3]);
    double k0 = atof (argv[4]), k1 = atof (argv[5]), k2 = atof (argv[6]);
    int law = argc > 7 ? atoi (argv[7]) : 0;
    ResonatorChannel ch; ch.prepare (fs);
    ch.setParams ({ k0, k1, k2 }, emph, (Mode) mode, (FreqLaw) law); ch.snap();
    const int N = 1 << 17;
    for (int n = 0; n < N; ++n) printf ("%.9g\n", ch.process (n == 0 ? 1.0f : 0.0f));
}
