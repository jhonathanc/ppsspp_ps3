// Copyright (c) 2012- PPSSPP Project.
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2 or later.

// Fast variant for the batched hardware path: the same model and color arithmetic, with plain float
// vector math instead of the GE's exact float24 operations. worldpos is the vertex in world space;
// worldnormal is normalized (or zero).
static inline float FastDot(const Vec3f &a, const Vec3f &b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float FastNormalize(Vec3f &v) {
	const float len2 = FastDot(v, v);
	if (len2 <= 0.0f)
		return 0.0f;
	const float len = sqrtf(len2);
	const float inv = 1.0f / len;
	v.x *= inv; v.y *= inv; v.z *= inv;
	return len;
}

static float FastShadeMapCoord(int l, const Vec3f &worldpos, const Vec3f &worldnormal, const Vec3f &viewDir) {
	Vec3f L(getFloat24(gstate.lpos[l * 3]), getFloat24(gstate.lpos[l * 3 + 1]), getFloat24(gstate.lpos[l * 3 + 2]));
	if (gstate.getLightType(l) != GE_LIGHTTYPE_DIRECTIONAL)
		L = L - worldpos;
	FastNormalize(L);
	if (gstate.isUsingSpecularLight(l)) {
		L = L + viewDir;
		FastNormalize(L);
	}
	return (FastDot(L, worldnormal) + 1.0f) * 0.5f;
}

void GenerateLightSTFast(VertexData &vertex, const Vec3f &worldpos, const Vec3f &worldnormal, const Vec3f &viewDir) {
	vertex.texturecoords.s() = FastShadeMapCoord(gstate.getUVLS0(), worldpos, worldnormal, viewDir);
	vertex.texturecoords.t() = FastShadeMapCoord(gstate.getUVLS1(), worldpos, worldnormal, viewDir);
}

void ProcessFast(VertexData &vertex, const Vec3f &worldpos, const Vec3f &worldnormal, const State &state) {
	Vec4<int> colorFactor;
	if (state.colorForAmbient || state.colorForDiffuse || state.colorForSpecular) {
		const Vec4<int> ones = Vec4<int>::AssignToAll(1);
		colorFactor = LightColorFactor(vertex.color0, ones);
	}

	const Vec4<int> mec = Vec4<int>::FromRGBA(gstate.getMaterialEmissive());
	const Vec4<int> mac = state.colorForAmbient ? colorFactor : state.material.ambientColorFactor;
	Vec4<int> final_color = mec + ((mac * state.baseAmbientColorFactor) >> 10);
	Vec4<int> specular_color = Vec4<int>::AssignToAll(0);

	for (unsigned int light = 0; light < 4; ++light) {
		const auto &lstate = state.lights[light];
		if (!lstate.enabled)
			continue;

		Vec3f L(lstate.pos[0], lstate.pos[1], lstate.pos[2]);
		float att = 1.0f;
		float spot = 1.0f;
		if (!lstate.directional) {
			L = L - worldpos;
			const float d2 = FastDot(L, L);
			const float d = FastNormalize(L);
			if (d == 0.0f)
				L = Vec3f(0.0f, 0.0f, 1.0f);
			const float den = lstate.att[0] + lstate.att[1] * d + lstate.att[2] * d2;
			att = den > 0.0f ? 1.0f / den : 0.0f;
			if (!(att > 0.0f))
				att = 0.0f;
			else if (att > 1.0f)
				att = 1.0f;
		}

		if (lstate.spot) {
			float rawSpot = FastDot(Vec3f(lstate.spotDir[0], lstate.spotDir[1], lstate.spotDir[2]), L) * lstate.spotDirRsqrt;
			if (std::isnan(rawSpot))
				rawSpot = std::signbit(rawSpot) ? 0.0f : 1.0f;
			spot = rawSpot >= lstate.spotCutoff ? GELightPow(rawSpot, lstate.spotExp) : 0.0f;
			if (std::isnan(spot))
				spot = 0.0f;
		}
		auto scaleAttSpot = [&](Vec4<int> c) {
			if (att < 1.0f)
				c = GELightColorScale(c, att);
			if (spot < 1.0f)
				c = GELightColorScale(c, spot);
			return c;
		};

		if (lstate.ambient)
			final_color += scaleAttSpot(GELightColorProduct(lstate.ambientColorFactor, mac));

		float diffuse_factor = 0.0f;
		if (lstate.diffuse || lstate.specular) {
			diffuse_factor = FastDot(L, worldnormal);
			if (lstate.poweredDiffuse)
				diffuse_factor = GELightPow(diffuse_factor, state.specularExp);
		}

		if (lstate.diffuse && diffuse_factor > 0.0f) {
			const Vec4<int> mdc = state.colorForDiffuse ? colorFactor : state.material.diffuseColorFactor;
			final_color += scaleAttSpot(GELightColorScale(GELightColorProduct(lstate.diffuseColorFactor, mdc), diffuse_factor));
		}

		if (lstate.specular && diffuse_factor >= 0.0f) {
			Vec3f H = L + Vec3f(state.viewDir[0], state.viewDir[1], state.viewDir[2]);
			if (FastNormalize(H) == 0.0f)
				H = Vec3f(0.0f, 0.0f, 1.0f);
			const float specular_factor = GELightPow(FastDot(H, worldnormal), state.specularExp);
			if (specular_factor > 0.0f) {
				const Vec4<int> msc = state.colorForSpecular ? colorFactor : state.material.specularColorFactor;
				specular_color += scaleAttSpot(GELightColorScale(GELightColorProduct(lstate.specularColorFactor, msc), specular_factor));
			}
		}
	}

	if (state.setColor1) {
		vertex.color0 = final_color.ToRGBA();
		vertex.color1 = specular_color.rgb().ToRGB();
	} else if (state.addColor1) {
		vertex.color0 = (final_color + specular_color).ToRGBA();
	} else {
		vertex.color0 = final_color.ToRGBA();
	}
}

