#pragma once
class Renderer;
class ShadowMap;
// Smoke-test contact sheets exercise every asset in both rendering passes.
void captureModelPreviews(Renderer& renderer, ShadowMap& shadows, int width, int height);
