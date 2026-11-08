// Plays every factory preset, every Easy tile, and combinations of them (voice x rhythm x
// auto-tune, voice x vintage, voice x effects), then a random walk of 400 switches like
// someone clicking around, and checks each sound is clearly heard and never jumps far
// louder or quieter than your raw voice. Rhythm gates are judged by their peaks, since
// chopping the sound lowers its average level by design.
#include "../Source/PluginProcessor.h"
#include "../Source/EasySteps.h"
#include "../Source/dsp/Chords.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
using namespace juce;
static const double sr = 48000; static const int bs = 256;
struct Rig {
    VocoderProcessor proc; bool chordPlaying = false, robotWas = false; int seen = 0;
    double ph = 0, t = 0; Random rng { 1 };
    Rig() { proc.setPlayConfigDetails (2,2,sr,bs); proc.prepareToPlay (sr,bs); seen = proc.getSoundChanges(); }
    float val (const char* id) { return proc.params.getRawParameterValue (id)->load(); }
    void watch() {
        bool robot = val ("robotOn") > 0.5f; bool picked = proc.getSoundChanges() != seen; seen = proc.getSoundChanges();
        if (robot && (! robotWas || picked) && val ("singToPlay") < 0.5f && ! chordPlaying) {
            Array<int> n; for (int x : vox::Chords::build ((int) val ("chordKey"), (int) val ("chordScale"), 0, (int) val ("chordFlavour"))) n.add (x);
            proc.requestChord (n, true); chordPlaying = true; }
        robotWas = robot;
    }
    // returns rms, peak of settled second half
    std::pair<double,float> level (double seconds) {
        AudioBuffer<float> b (2, bs); double sum = 0; long n = 0; float peak = 0;
        int blocks = (int) (seconds * sr / bs);
        for (int k = 0; k < blocks; ++k) {
            for (int i = 0; i < bs; ++i) { ph += 140.0/sr; t += 1.0/sr;
                float saw = (float) (2.0*(ph-std::floor(ph))-1.0);
                float env = (float) (0.5+0.5*std::sin (t*MathConstants<double>::twoPi*3.0));
                float v = 0.3f*env*saw + 0.01f*(rng.nextFloat()*2-1); b.setSample (0,i,v); b.setSample (1,i,v); }
            MidiBuffer m; proc.processBlock (b, m);
            if (k >= blocks/2) for (int i = 0; i < bs; ++i, ++n) { float y = b.getSample (0,i), z = b.getSample (1,i); sum += 0.5*(y*y+z*z); peak = std::max (peak, std::max (std::abs (y), std::abs (z))); }
        }
        return { std::sqrt (sum / std::max (1L,n)), peak };
    }
    const EasySteps::Step* step (const char* title) { for (auto& s : EasySteps::all()) if (String (s.title) == title) return &s; return nullptr; }
    void tile (const EasySteps::Step& s, const EasySteps::Tile& tl) {
        EasySteps::apply (proc.params, s, tl); if (String (s.title) == "Voice") proc.soundPicked(); watch(); }
};
int main()
{
    ScopedJuceInitialiser_GUI juce;
    double raw; { Rig r; raw = r.level (1.5).first; }
    std::printf ("raw voice rms %.4f\n", raw);
    int flagged = 0, total = 0;
    auto judge = [&] (const String& what, std::pair<double,float> lv, bool gated) {
        double db = Decibels::gainToDecibels (lv.first / raw);
        bool bad = (gated ? lv.second < 0.08f : db < -9.0) || db > 7.0 || lv.second > 0.97f;
        ++total; if (bad) { ++flagged; std::printf ("FLAG %-60s %+6.1f dB peak %.2f\n", what.toRawUTF8(), db, lv.second); }
        return db;
    };
    {
    // A: presets
    std::printf ("== presets\n");
    for (int p = 0; ; ++p) { Rig r; if (p >= r.proc.getNumPrograms()) break; r.proc.setCurrentProgram (p); r.watch();
        auto lv = r.level (2.0); double db = judge ("preset " + r.proc.getProgramName (p), lv, r.val ("arpOn") > 0.5f && r.val ("arpMode") > 1.5f);
        std::printf ("  %-22s %+6.1f dB  peak %.2f\n", r.proc.getProgramName (p).toRawUTF8(), db, lv.second); }
    // B: tiles alone
    std::printf ("== tiles\n");
    for (auto& s : EasySteps::all()) for (auto& tl : s.tiles) { Rig r; r.tile (s, tl); auto lv = r.level (2.0);
        double db = judge (String (s.title) + "/" + tl.name, lv, String (s.title) == "Rhythm");
        std::printf ("  %-10s %-12s %+6.1f dB peak %.2f\n", s.title, tl.name, db, lv.second); }
    // C: combos voice x rhythm x tune
    std::printf ("== voice x rhythm x autotune\n");
    { Rig tmp; auto* V = tmp.step ("Voice"); auto* R = tmp.step ("Rhythm"); auto* T = tmp.step ("Auto-Tune"); auto* Vi = tmp.step ("Vintage"); auto* E = tmp.step ("Effects");
      double lo = 99, hi = -99;
      for (auto& v : V->tiles) for (auto& rh : R->tiles) for (int ti : { 0, 2, 4 }) { auto& tu = T->tiles[(size_t) ti];
          Rig r; r.tile (*V, v); r.tile (*R, rh); r.tile (*T, tu); auto lv = r.level (1.2);
          double db = judge (String ("combo ") + v.name + " + " + rh.name + " + " + tu.name, lv, true); lo = std::min (lo, db); hi = std::max (hi, db); }
      std::printf ("  range %.1f .. %.1f dB\n", lo, hi);
      lo = 99; hi = -99;
      for (auto& v : V->tiles) for (auto& vi : Vi->tiles) { Rig r; r.tile (*V, v); r.tile (*Vi, vi); auto lv = r.level (1.2);
          double db = judge (String ("combo ") + v.name + " + vintage " + vi.name, lv, false); lo = std::min (lo, db); hi = std::max (hi, db); }
      std::printf ("  voice x vintage range %.1f .. %.1f dB\n", lo, hi);
      lo = 99; hi = -99;
      for (auto& v : V->tiles) { Rig r; r.tile (*V, v); for (auto& e : E->tiles) r.tile (*E, e); auto lv = r.level (1.5);
          double db = judge (String ("combo ") + v.name + " + all effects", lv, false); lo = std::min (lo, db); hi = std::max (hi, db);
          for (auto& e : E->tiles) { Rig q; q.tile (*V, v); q.tile (*E, e); auto l2 = q.level (1.2);
              double d2 = judge (String ("combo ") + v.name + " + " + e.name, l2, false); lo = std::min (lo, d2); hi = std::max (hi, d2); } }
      std::printf ("  voice x effects range %.1f .. %.1f dB\n", lo, hi);
    }
    }
    // D: random walk of switches
    std::printf ("== random walk\n");
    { Rig r; Random pick (42); double lo = 99, hi = -99;
      for (int i = 0; i < 400; ++i) {
          String what; int kind = pick.nextInt (10);
          if (kind == 0) { int p = pick.nextInt (r.proc.getNumPrograms()); r.proc.setCurrentProgram (p); r.watch(); what = "preset " + r.proc.getProgramName (p); }
          else if (kind == 1) { const char* ids[] = { "voiceOn", "robotOn", "arpOn", "fxOn", "vintageOn", "tuneOn" }; auto id = ids[pick.nextInt (6)];
              EasySteps::set (r.proc.params, id, r.val (id) > 0.5f ? 0.f : 1.f); r.watch(); what = String ("bar ") + id; }
          else if (kind == 2) { r.proc.requestChord ({}, true); r.chordPlaying = false; what = "stop chords"; }
          else if (kind == 3) { r.proc.setCurrentProgram (0); r.proc.requestChord ({}, true); r.chordPlaying = false; what = "raw voice button"; }
          else { auto& steps = EasySteps::all(); auto& s = steps[(size_t) pick.nextInt ((int) steps.size())]; auto& tl = s.tiles[(size_t) pick.nextInt ((int) s.tiles.size())];
              // a lit pick-one tile clicked again goes back to the first tile, as in the app
              if (! s.switches && ! s.group.empty() && &tl != &s.tiles.front() && EasySteps::isActive (r.proc.params, s, tl)) r.tile (s, s.tiles.front()); else r.tile (s, tl);
              what = String (s.title) + "/" + tl.name; }
          auto lv = r.level (1.5); double db = judge ("walk " + String (i) + ": " + what + String::formatted (" [v%d r%d a%d m%d f%d t%d s%d vin%d]", (int) r.val ("voiceOn"), (int) r.val ("robotOn"), (int) r.val ("arpOn"), (int) r.val ("arpMode"), (int) r.val ("fxOn"), (int) r.val ("tuneOn"), (int) r.val ("singToPlay"), (int) r.val ("vintageOn")), lv, true);
          lo = std::min (lo, db); hi = std::max (hi, db); }
      std::printf ("  walk range %.1f .. %.1f dB\n", lo, hi); }
    std::printf ("\n%d of %d flagged\n", flagged, total);
    return flagged == 0 ? 0 : 1;
}
