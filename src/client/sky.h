/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 3.0 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "irrlichttypes_extrabloated.h"
#include <ISceneNode.h>
#include <array>
#include "camera.h"
#include "irr_ptr.h"
#include "shader.h"
#include "skyparams.h"

#pragma once

#define SKY_MATERIAL_COUNT 12

// Menu sky
class Sky;
extern Sky *g_menusky;

class ITextureSource;

// Skybox, rendered with zbuffer turned off, before all other nodes.
class Sky : public scene::ISceneNode
{
public:
	//! constructor
	Sky(s32 id, ITextureSource *tsrc, IShaderSource *ssrc);
	Sky(s32 id, ITextureSource *tsrc, IShaderSource *ssrc,
			scene::ISceneManager *smgr, bool is_mainmenu = false);

	virtual ~Sky();

	virtual void OnRegisterSceneNode();

	//! renders the node.
	virtual void render();

	virtual const aabb3f &getBoundingBox() const { return m_box; }

	// Used by Irrlicht for optimizing rendering
	virtual video::SMaterial &getMaterial(u32 i) { return m_materials[i]; }
	virtual u32 getMaterialCount() const { return SKY_MATERIAL_COUNT; }

	void update(float m_time_of_day, float time_brightness, float direct_brightness,
			bool sunlight_seen, CameraMode cam_mode, float yaw, float pitch);

	float getBrightness() { return m_brightness; }

	const video::SColor &getBgColor() const
	{
		return m_visible ? m_bgcolor : m_fallback_bg_color;
	}

	const video::SColor &getSkyColor() const
	{
		return m_visible ? m_skycolor : m_fallback_bg_color;
	}

	void setSunVisible(bool sun_visible) { m_sun_params.visible = sun_visible; }
	void setSunTexture(const std::string &sun_texture,
		const std::string &sun_tonemap, ITextureSource *tsrc);
	void setSunScale(f32 sun_scale) { m_sun_params.scale = sun_scale; }
	void setSunriseVisible(bool glow_visible) { m_sun_params.sunrise_visible = glow_visible; }
	void setSunriseTexture(const std::string &sunglow_texture, ITextureSource* tsrc);

	void setMoonVisible(bool moon_visible) { m_moon_params.visible = moon_visible; }
	void setMoonTexture(const std::string &moon_texture,
		const std::string &moon_tonemap, ITextureSource *tsrc);
	void setMoonScale(f32 moon_scale) { m_moon_params.scale = moon_scale; }

	void setStarsVisible(bool stars_visible) { m_star_params.visible = stars_visible; }
	void setStarCount(u16 star_count, bool force_update);
	void setStarColor(video::SColor star_color) { m_star_params.starcolor = star_color; }
	void setStarScale(f32 star_scale);

	bool getCloudsVisible() const { return m_clouds_visible && m_clouds_enabled; }
	const video::SColorf &getCloudColor() const { return m_cloudcolor_f; }

	void setVisible(bool visible) { m_visible = visible; }
	// Set only from set_sky API
	void setCloudsEnabled(bool clouds_enabled) { m_clouds_enabled = clouds_enabled; }
	void setFallbackBgColor(video::SColor fallback_bg_color)
	{
		m_fallback_bg_color = fallback_bg_color;
	}
	void overrideColors(video::SColor bgcolor, video::SColor skycolor)
	{
		m_bgcolor = bgcolor;
		m_skycolor = skycolor;
	}
	void setSkyColors(const SkyColor &sky_color);
	void setHorizonTint(video::SColor sun_tint, video::SColor moon_tint,
		const std::string &use_sun_tint);
	void setInClouds(bool clouds) { m_in_clouds = clouds; }
	void clearSkyboxTextures() { m_sky_params.textures.clear(); }
	void addTextureToSkybox(const  std::string &texture, int material_id,
		ITextureSource *tsrc);
	const video::SColorf &getCurrentStarColor() const { return m_star_color; }

	void setCustomSkyBodyPos(float moon_horizon_pos, float moon_day_pos,
			float moon_angle, float sun_horizon_pos, float sun_day_pos,
			float sun_angle);

private:
	aabb3f m_box;
	video::SMaterial m_materials[SKY_MATERIAL_COUNT];
	// How much sun & moon transition should affect horizon color
	float m_horizon_blend()
	{
		if (!m_sunlight_seen)
			return 0;
		float x = m_time_of_day >= 0.5 ? (1 - m_time_of_day) * 2
					       : m_time_of_day * 2;

		if (x <= 0.3)
			return 0;
		if (x <= 0.4) // when the sun and moon are aligned
			return (x - 0.3) * 10;
		if (x <= 0.5)
			return (0.5 - x) * 10;
		return 0;
	}

	// Mix two colors by a given amount
	static video::SColor m_mix_scolor(video::SColor col1, video::SColor col2, f32 factor)
	{
		video::SColor result = video::SColor(
				col1.getAlpha() * (1 - factor) + col2.getAlpha() * factor,
				col1.getRed() * (1 - factor) + col2.getRed() * factor,
				col1.getGreen() * (1 - factor) + col2.getGreen() * factor,
				col1.getBlue() * (1 - factor) + col2.getBlue() * factor);
		return result;
	}
	static video::SColorf m_mix_scolorf(video::SColorf col1, video::SColorf col2, f32 factor)
	{
		video::SColorf result =
				video::SColorf(col1.r * (1 - factor) + col2.r * factor,
						col1.g * (1 - factor) + col2.g * factor,
						col1.b * (1 - factor) + col2.b * factor,
						col1.a * (1 - factor) + col2.a * factor);
		return result;
	}

	bool m_visible = true;
	// Used when m_visible=false
	video::SColor m_fallback_bg_color = video::SColor(255, 255, 255, 255);
	bool m_first_update = true;
	float m_time_of_day;
	float m_time_brightness;
	bool m_sunlight_seen;
	float m_brightness = 0.5f;
	float m_cloud_brightness = 0.5f;
	bool m_clouds_visible; // Whether clouds are disabled due to player underground
	bool m_clouds_enabled = true; // Initialised to true, reset only by set_sky API
	bool m_directional_colored_fog;
	bool m_in_clouds = true; // Prevent duplicating bools to remember old values
	bool m_enable_shaders = false;

	video::SColorf m_bgcolor_bright_f = video::SColorf(1.0f, 1.0f, 1.0f, 1.0f);
	video::SColorf m_skycolor_bright_f = video::SColorf(1.0f, 1.0f, 1.0f, 1.0f);
	video::SColorf m_cloudcolor_bright_f = video::SColorf(1.0f, 1.0f, 1.0f, 1.0f);
	video::SColor m_bgcolor;
	video::SColor m_skycolor;
	video::SColorf m_cloudcolor_f;

	// pure white: becomes "diffuse light component" for clouds
	video::SColorf m_cloudcolor_day_f = video::SColorf(1, 1, 1, 1);
	// dawn-factoring version of pure white (note: R is above 1.0)
	video::SColorf m_cloudcolor_dawn_f = video::SColorf(
		255.0f/240.0f,
		223.0f/240.0f,
		191.0f/255.0f
	);

	SkyboxParams m_sky_params;
	SunParams m_sun_params;
	MoonParams m_moon_params;
	StarParams m_star_params;

	bool m_default_tint = true;

	u64 m_seed = 0;
	irr_ptr<scene::SMeshBuffer> m_stars;
	video::SColorf m_star_color;

	video::ITexture *m_sun_texture;
	video::ITexture *m_moon_texture;
	video::ITexture *m_sun_tonemap;
	video::ITexture *m_moon_tonemap;

	bool m_use_custom_sky_body_pos = false;
	float m_custom_moon_horizon_pos = -90;
	float m_custom_moon_day_pos = -90;
	float m_custom_moon_angle = 0;
	float m_custom_sun_horizon_pos = -90;
	float m_custom_sun_day_pos = -90;
	float m_custom_sun_angle = 0;

	bool m_is_mainmenu = false;

	void updateStars();

	void draw_sun(video::IVideoDriver *driver, float sunsize, const video::SColor &suncolor,
		const video::SColor &suncolor2, float wicked_time_of_day);
	void draw_moon(video::IVideoDriver *driver, float moonsize, const video::SColor &mooncolor,
		const video::SColor &mooncolor2, float wicked_time_of_day);
	void draw_sky_body(std::array<video::S3DVertex, 4> &vertices,
		float pos_1, float pos_2, const video::SColor &c);
	void draw_stars(video::IVideoDriver *driver, float wicked_time_of_day);
	void place_sky_body(std::array<video::S3DVertex, 4> &vertices,
		float horizon_position,	float day_position, float angle = 0);
	void setSkyDefaults();
};
