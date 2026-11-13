/***
 *
 *	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

#include <algorithm>

#include "cbase.h"
#include "MaterialSystem.h"

#include "networking/NetworkDataSystem.h"

#include "utils/JSONSystem.h"

constexpr std::size_t MinimumMaterialsCount = 512; // original max number of textures loaded

constexpr std::string_view MaterialsConfigSchemaName{"MaterialsConfig"sv};

static std::string GetMaterialsConfigSchema()
{
	return fmt::format(R"(
{{
	"$schema": "http://json-schema.org/draft-07/schema#",
	"title": "Materials Definition File",
	"type": "object",
	"patternProperties": {{
		"^\\w+$": {{
			"type": "object",
			"properties": {{
				"Type": {{
					"type": "string"
				}},
				"Impact": {{
					"type": "string"
				}}
			}},
			"anyOf": [{{"required": ["Type"]}}, {{"required": ["Impact"]}}]
		}}
	}},
	"additionalProperties": false
}}
)");
}

bool MaterialSystem::Initialize()
{
	m_Logger = g_Logging.CreateLogger("materials");

	g_JSON.RegisterSchema(MaterialsConfigSchemaName, &GetMaterialsConfigSchema);

	g_NetworkData.RegisterHandler("MaterialSystem", this);

	return true;
}

void MaterialSystem::Shutdown()
{
	g_Logging.RemoveLogger(m_Logger);
	m_Logger.reset();
}

void MaterialSystem::HandleNetworkDataBlock(NetworkDataBlock& block)
{
	if (block.Mode == NetworkDataMode::Serialize)
	{
		// This configuration structure must be identical to the one loaded from files!
		block.Data = json::object();

		for (const auto& material : m_Materials)
		{
			json mat = json::object();

			if (material.second.Type != 0)
				mat.emplace("Type", std::string{material.second.Type});

			if (material.second.Impact != 0)
				mat.emplace("Impact", std::string{material.second.Impact});

			block.Data.emplace(material.first.c_str(), std::move(mat));
		}

		g_NetworkData.GetLogger()->debug("Wrote {} materials to network data", m_Materials.size());
	}
	else
	{
		m_Materials.clear();

		if (!ParseConfiguration(block.Data))
		{
			block.ErrorMessage = "Material network data has invalid format";
			return;
		}

		g_NetworkData.GetLogger()->debug("Parsed {} materials from network data", m_Materials.size());
	}
}

void MaterialSystem::LoadMaterials(std::span<const std::string> fileNames)
{
	m_Materials.clear();
	m_Materials.reserve(MinimumMaterialsCount);

	for (const auto& fileName : fileNames)
	{
		m_Logger->trace("Loading {}", fileName);

		if (const auto result = g_JSON.ParseJSONFile(fileName.c_str(),
				{.SchemaName = MaterialsConfigSchemaName},
				[this](const json& input)
				{ return ParseConfiguration(input); });
			!result.value_or(false))
		{
			m_Logger->error("Error loading materials configuration file \"{}\"", fileName);
		}
	}

	m_Logger->debug("Loaded {} materials", m_Materials.size());
}

const char* MaterialSystem::StripTexturePrefix(const char* name)
{
	// strip leading '-0' or '+0~' or '{' or '!'
	if (*name == '-' || *name == '+')
	{
		name += 2;
	}

	if (*name == '{' || *name == '!' || *name == '~' || *name == ' ')
	{
		++name;
	}

	return name;
}

char MaterialSystem::FindTextureType(const char* name) const
{
	TextureName upperName{name};

	std::transform(upperName.begin(), upperName.end(), upperName.begin(), [](char c)
		{ return std::toupper(c); });

	if (auto it = m_Materials.find(upperName); it != m_Materials.end() && it->second.Type != 0)
	{
		return it->second.Type;
	}

	return CHAR_TEX_CONCRETE;
}

char MaterialSystem::FindImpactType(const char* name) const
{
	TextureName upperName{name};

	std::transform(upperName.begin(), upperName.end(), upperName.begin(), [](char c)
		{ return std::toupper(c); });

	if (auto it = m_Materials.find(upperName); it != m_Materials.end())
	{
		if (it->second.Impact != 0)
			return it->second.Impact;

		if (it->second.Type != 0)
			return it->second.Type;
	}

	return CHAR_TEX_CONCRETE;
}

bool MaterialSystem::ParseConfiguration(const json& input)
{
	if (!input.is_object())
	{
		return false;
	}

	for (const auto& [texture, mat] : input.items())
	{
		if (!mat.is_object())
		{
			return false;
		}

		const auto type = mat.value<std::string>("Type", {});
		const auto impact = mat.value<std::string>("Impact", {});

		if (texture.empty() || (type.empty() && impact.empty()))
		{
			return false;
		}

		TextureName name{texture.c_str()};

		std::transform(name.begin(), name.end(), name.begin(), [](char c)
			{ return std::toupper(c); });

		// johnhalflife: merged into what an earlier file gave
		Material& material = m_Materials[std::move(name)];

		if (!type.empty())
			material.Type = type.front();

		if (!impact.empty())
			material.Impact = impact.front();
	}

	return true;
}
