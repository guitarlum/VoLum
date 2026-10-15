#pragma once

// Factory hero art by fractal case, and the animator for it. BUILD and the PLAY
// static path draw through DrawFactoryHeroArt (via DrawHeroFractalArt); PLAY
// asks MakeFactoryArtAnimator, which answers nullptr unless the art's registry
// flag is on (debug captures may ignore the flag).

#include "VoLumArtAmpeteOne.h"
#include "VoLumArtBadCat.h"
#include "VoLumArtBrunetti.h"
#include "VoLumArtCustom.h"
#include "VoLumArtDiezelHerbert.h"
#include "VoLumArtFryette.h"
#include "VoLumArtHkTriamp.h"
#include "VoLumArtJmp2203.h"
#include "VoLumArtJvm210.h"
#include "VoLumArtLichtlaerm.h"
#include "VoLumArtMarshall2204.h"
#include "VoLumArtOrangeOd120.h"
#include "VoLumArtOrangeOrs100.h"
#include "VoLumArtRegistry.h"
#include "VoLumArtSebago.h"
#include "VoLumArtSoldano.h"
#include "VoLumArtThcSunset.h"

namespace volumart
{
inline void DrawFactoryHeroArt(IGraphics& g, const IRECT& r, int fractalCase)
{
  switch (fractalCase)
  {
    case 0: art::DrawAmpeteOneHero(g, r); break;
    case 1: art::DrawBadCatHero(g, r); break;
    case 2: art::DrawBrunettiHero(g, r); break;
    case 3: art::DrawFryetteHero(g, r); break;
    case 4: art::DrawHkTriampHero(g, r); break;
    case 5: art::DrawLichtlaermHero(g, r); break;
    case 6: art::DrawMarshall2204Hero(g, r); break;
    case 7: art::DrawJmp2203Hero(g, r); break;
    case 8: art::DrawJvm210Hero(g, r); break;
    case 9: art::DrawOrangeOd120Hero(g, r); break;
    case 10: art::DrawOrangeOrs100Hero(g, r); break;
    case 11: art::DrawSebagoHero(g, r); break;
    case 12: art::DrawSoldanoHero(g, r); break;
    case 13: art::DrawThcSunsetHero(g, r); break;
    case 14: art::DrawDiezelHerbertHero(g, r); break;
    default: break;
  }
}

inline std::unique_ptr<ArtAnimator> MakeFactoryArtAnimator(int fractalCase, bool ignoreFlag)
{
  if (!ignoreFlag && !ArtAnimates(fractalCase))
    return nullptr;
  switch (fractalCase)
  {
    case 0: return art::MakeAmpeteOneAnimator();
    case 1: return art::MakeBadCatAnimator();
    case 2: return art::MakeBrunettiAnimator();
    case 3: return art::MakeFryetteAnimator();
    case 4: return art::MakeHkTriampAnimator();
    case 5: return art::MakeLichtlaermAnimator();
    case 6: return art::MakeMarshall2204Animator();
    case 7: return art::MakeJmp2203Animator();
    case 8: return art::MakeJvm210Animator();
    case 9: return art::MakeOrangeOd120Animator();
    case 10: return art::MakeOrangeOrs100Animator();
    case 11: return art::MakeSebagoAnimator();
    case 12: return art::MakeSoldanoAnimator();
    case 13: return art::MakeThcSunsetAnimator();
    case 14: return art::MakeDiezelHerbertAnimator();
    default: return nullptr;
  }
}

inline std::unique_ptr<ArtAnimator> MakeCustomArtAnimator(int style, bool ignoreFlag, art::CustomArtPaint paint)
{
  if (!ignoreFlag && !kCustomArtAnimate)
    return nullptr;
  return std::make_unique<art::CustomArtAnimator>(style, std::move(paint));
}
} // namespace volumart
