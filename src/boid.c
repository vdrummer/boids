#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "boid.h"
#include "mathStuff.h"
#include "constants.h"
#include "gamestate.h"
#include "font.h"

void boid_render(Boid* b, SDL_Renderer* r, Gamestate* gs) {
  Vec2dD ptA = {BOID_LENGTH / 2, 0}; // the tip
  Vec2dD ptB = {-1 * (BOID_LENGTH / 2), BOID_WIDTH / 2}; // right rear
  Vec2dD ptC = {-1 * (BOID_LENGTH / 2), -1 * (BOID_WIDTH / 2)}; // left rear

  const Vec2dD xAxis = {1, 0};
  int angle = DEG(vec2dd_angle(xAxis, b->velocity));

  ptA = vec2dd_rotate(ptA, angle);
  ptB = vec2dd_rotate(ptB, angle);
  ptC = vec2dd_rotate(ptC, angle);

  Sint16 vx[3] = {
    b->position.x + ptA.x,
    b->position.x + ptB.x,
    b->position.x + ptC.x,
  };
  Sint16 vy[3] = {
    b->position.y + ptA.y,
    b->position.y + ptB.y,
    b->position.y + ptC.y,
  };

  const Uint32 color_normal = 0xffffffff; // white
  const Uint32 color_selected = 0xff0000ff; // red
  const Uint32 color_neighbor = 0xff00ffff; // yellow

  // draw boid
  if (gs->debugViewEnabled) {
    if (gs->debugView->activeBoidIndex > -1
        && b->index == gs->debugView->activeBoidIndex) {
      filledPolygonColor(r, vx, vy, 3, color_selected);
      circleColor(r, b->position.x, b->position.y, VIEW_RADIUS, 0xffffffff);
      circleColor(r, b->position.x, b->position.y, AVOIDANCE_RADIUS, 0xffffffff);

    } else if (gs->debugView->activeBoidNeighbors[b->index]) {
      filledPolygonColor(r, vx, vy, 3, color_neighbor);
    } else {
      filledPolygonColor(r, vx, vy, 3, color_normal);
    }
  } else {
    // debugView disabled
    filledPolygonColor(r, vx, vy, 3, color_normal);
  }

  if (gs->debugViewEnabled) {
    // draw center points of boids
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_Rect rect = {
      .x = b->position.x - 1,
      .y = b->position.y - 1,
      .h = 2,
      .w = 2
    };
    SDL_RenderFillRect(r, &rect);

    // draw boid's ID
    SDL_Color textColor = {255, 0, 0, 255};
    char textBuffer[4];
    sprintf(textBuffer, "%03d", b->index);
    textBuffer[3] = '\0'; // just in case...

    SDL_Texture* textTexture = createTextTexture(
        textColor,
        textBuffer,
        gs->debugView->font,
        r
    );

    SDL_Rect dstRect;
    SDL_QueryTexture(textTexture, NULL, NULL, &dstRect.w, &dstRect.h);
    dstRect.x = b->position.x + BOID_WIDTH;
    dstRect.y = b->position.y + BOID_WIDTH;

    SDL_RenderCopy(r, textTexture, NULL, &dstRect);
    SDL_DestroyTexture(textTexture);
  }
}

bool isInFov(Boid* b, Boid* other, Gamestate* gs) {
  if (!gs->fovEnabled) {
    return true;
  }

  const Vec2dD diff = vec2dd_add(other->position, vec2dd_multScalar(b->position, -1));
  double angle = abs((int) DEG(vec2dd_angle(b->velocity, diff)));
  return angle <= FIELD_OF_VIEW / 2;
}

void boid_update(Boid* b, Gamestate* gs) {
  // do boid stuff
  int neighborCount = 0;
  Vec2dD center = {0};
  Vec2dD neighborVelocity = {0};

  Vec2dD separationVector = {0};

  // iterate over all other boids
  // gather values needed for boid rule application
  for (int i = 0; i < NUM_BOIDS; i++) {
    if (i == b->index) continue;
    Boid other = gs->boids[i];
    const double dist = vec2dd_dist(b->position, other.position);

    if (dist <= VIEW_RADIUS && isInFov(b, &other, gs)) {
      neighborCount++;
      center.x += other.position.x;
      center.y += other.position.y;
      neighborVelocity.x += other.position.x;
      neighborVelocity.y += other.position.y;

      if (gs->debugViewEnabled && gs->debugView->activeBoidIndex == b->index) {
        // enable other in neighbor list
        gs->debugView->activeBoidNeighbors[i] = true;
        gs->debugView->neighborCount++;

        // add data to neighborCenter
        gs->debugView->neighborCenter.x += other.position.x;
        gs->debugView->neighborCenter.y += other.position.y;
      }

      if (dist <= AVOIDANCE_RADIUS) {
        Vec2dD pos_diff = vec2dd_add(other.position, vec2dd_multScalar(b->position, -1));
        separationVector = vec2dd_add(separationVector, vec2dd_multScalar(pos_diff, -1));
      }
    }
  }

  // don't compute avg values if division of zero would occur
  if (neighborCount > 0) {
    center.x /= neighborCount;
    center.y /= neighborCount;

    neighborVelocity.x /= neighborCount;
    neighborVelocity.y /= neighborCount;
  }

  // apply weights for boid rules
  Vec2dD target = vec2dd_subtract(center, b->position);
  target = vec2dd_multScalar(target, COHESION_FACTOR);

  neighborVelocity = vec2dd_add(neighborVelocity, vec2dd_multScalar(b->velocity, -1));
  neighborVelocity = vec2dd_multScalar(neighborVelocity, ALIGNMENT_FACTOR);

  separationVector = vec2dd_multScalar(separationVector, SEPARATION_FACTOR);

  if (gs->cohesionEnabled)
    b->velocity = vec2dd_add(b->velocity, target);
  if (gs->alignmentEnabled)
    b->velocity = vec2dd_add(b->velocity, neighborVelocity);
  if (gs->separationEnabled)
    b->velocity = vec2dd_add(b->velocity, separationVector);

  // limit to MAX_SPEED
  const double vel_abs = vec2dd_length(b->velocity);
  if (vel_abs > MAX_SPEED) {
    b->velocity = vec2dd_multScalar(b->velocity, 1.0 / vel_abs);
    b->velocity = vec2dd_multScalar(b->velocity, MAX_SPEED);
  }

  // add velocity to position
  b->position = vec2dd_add(b->position, b->velocity);

  // handle window edges
  if (b->position.y < 0) b->position.y = WINDOW_HEIGHT - 1;
  if (b->position.y > WINDOW_HEIGHT) b->position.y = 0;

  if (b->position.x < 0) b->position.x = WINDOW_WIDTH - 1;
  if (b->position.x > WINDOW_WIDTH) b->position.x = 0;

}

Boid boid_init(int index) {
  Boid b = {0};
  b.position.x = rand() % WINDOW_WIDTH;
  b.position.y = rand() % WINDOW_HEIGHT;
  b.index = index;

  b.velocity.x = rand() % MAX_SPEED + 1;
  if (rand() % 2) {
    b.velocity.x *= -1;
  }
  b.velocity.y = rand() % MAX_SPEED + 1;
  if (rand() % 2) {
    b.velocity.y *= -1;
  }
  return b;
}

void boid_randomizeVelocity(Boid* b) {
  b->velocity.x = rand() % MAX_SPEED + 1;
  if (rand() % 2) {
    b->velocity.x *= -1;
  }
  b->velocity.y = rand() % MAX_SPEED + 1;
  if (rand() % 2) {
    b->velocity.y *= -1;
  }
}
