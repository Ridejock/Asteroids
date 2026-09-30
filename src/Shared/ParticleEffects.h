#pragma once

#include <Emerald/Particles/ParticleSystem.h>
#include <Emerald/Renderer/Renderer2D.h>

#include "GameMode.h"

namespace Asteroids {

// The optional eye candy on top of the game (Options > Particles): sparks and dust when rocks
// break, a big burst when the ship explodes, debris from a hit saucer and the engine's exhaust.
// Purely visual: it reads the game (its sounds say what happened, and where) but never changes
// it, and has its own random numbers, so games play exactly the same with it on or off.
class ParticleEffects {
public:
    // Off: no new particles, and the current ones disappear.
    void SetEnabled(bool enabled);

    // After every Game::Update (before the game's sounds are cleared): spawns the effects for
    // this step's events and the exhaust, then moves all particles.
    void Update(const GameMode& game, f32 dt);
    void Draw(Emerald::Renderer2D& r, const Emerald::ParticleDrawOptions& options) const
    {
        m_Particles.Draw(r, options);
    }
    void Clear() { m_Particles.Clear(); }
    // The effect for one event (Update calls it for each of the step's sounds).
    void OnSound(const GameSound& sound);

    [[nodiscard]] const Emerald::ParticleSystem& GetParticles() const { return m_Particles; }

private:
    // Enough for a ship explosion next to a few breaking rocks (~130 each) plus the exhaust;
    // more would just be dropped.
    static constexpr u32 kCapacity = 2048;
    Emerald::ParticleSystem m_Particles{kCapacity};
    Emerald::ContinuousEmitter m_Exhaust;
    bool m_Enabled = true;
};

} // namespace Asteroids
