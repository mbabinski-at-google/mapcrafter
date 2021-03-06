/*
 * Copyright 2012-2016 Moritz Hilscher
 *
 * This file is part of Mapcrafter.
 *
 * Mapcrafter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mapcrafter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Mapcrafter.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tilerenderer.h"

#include "../../biomes.h"
#include "../../blockimages.h"
#include "../../image.h"
#include "../../rendermode.h"
#include "../../tileset.h"
#include "../../../mc/pos.h"
#include "../../../mc/worldcache.h"
#include "../../../util.h"

#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace mapcrafter {
namespace renderer {

TopdownTileRenderer::TopdownTileRenderer(const RenderView* render_view,
		BlockImages* images, int tile_width, mc::WorldCache* world,
		RenderMode* render_mode)
	: TileRenderer(render_view, images, tile_width, world, render_mode) {
}

TopdownTileRenderer::~TopdownTileRenderer() {
}

namespace {

struct RenderBlock {
	RGBAImage block;
	uint16_t id, data;
	mc::BlockPos pos;
};

}

void TopdownTileRenderer::renderChunk(const mc::Chunk& chunk, RGBAImage& tile, int dx, int dy) {
	// TODO implement preblit water render behavior

	int texture_size = images->getTextureSize();

	for (int x = 0; x < 16; x++) {
		for (int z = 0; z < 16; z++) {
			std::deque<RenderBlock> blocks;

			// TODO make this water thing a bit nicer
			bool in_water = false;
			int water = 0;

			mc::LocalBlockPos localpos(x, z, 0);
			//int height = chunk.getHeightAt(localpos);
			//localpos.y = height;
			localpos.y = -1;
			if (localpos.y >= mc::CHUNK_HEIGHT*16 || localpos.y < 0)
				localpos.y = mc::CHUNK_HEIGHT*16 - 1;

			uint16_t id = chunk.getBlockID(localpos);
			while (id == 0 && localpos.y > 0) {
				localpos.y--;
				id = chunk.getBlockID(localpos);
			}
			if (localpos.y < 0)
				continue;

			while (localpos.y >= 0) {
				mc::BlockPos globalpos = localpos.toGlobalPos(chunk.getPos());

				uint16_t id;
				id = current_chunk->getBlockID(localpos);

				if (id == 0) {
					in_water = false;
					localpos.y--;
					continue;
				}

				uint16_t data, extra_data;
				data = current_chunk->getBlockData(localpos);
				extra_data = current_chunk->getBlockExtraData(localpos, id);

				bool is_water = (id == 8 || id == 9) && data == 0;

				if (render_mode->isHidden(globalpos, id, data)) {
					localpos.y--;
					continue;
				}

				if (is_water && !use_preblit_water) {
					if (is_water == in_water) {
						localpos.y--;
						continue;
					}
					in_water = is_water;
				} else if (use_preblit_water) {
					if (!is_water)
						water = 0;
					else {
						water++;
						if (water > images->getMaxWaterPreblit()) {
							auto it = blocks.begin();
							while (it != blocks.end()) {
								auto current = it++;
								if (it == blocks.end() || (it->id != 8 && it->id != 9)) {
									RenderBlock& top = *current;
									// blocks.erase(current);

									top.id = 8;
									top.data = OPAQUE_WATER;
									top.block = images->getBlock(top.id, top.data);
									render_mode->draw(top.block, top.pos, top.id, top.data);
									// blocks.insert(current, top);
									break;
								} else {
									blocks.erase(current);
								}
							}
							break;
						}
					}
				}

				data = checkNeighbors(globalpos, id, data);

				RGBAImage block;
				if (Biome::isBiomeBlock(id, data)) {
					block = images->getBiomeBlock(id, data, getBiomeOfBlock(globalpos, &chunk), extra_data);
				} else {
					block = images->getBlock(id, data, extra_data);
				}

				render_mode->draw(block, globalpos, id, data);
				RenderBlock render_block;
				render_block.block = block;
				render_block.id = id;
				render_block.data = data;
				render_block.pos = globalpos;
				blocks.push_back(render_block);

				if (!images->isBlockTransparent(id, data))
					break;
				localpos.y--;
			}

			while (blocks.size() > 0) {
				RenderBlock render_block = blocks.back();
				tile.alphaBlit(render_block.block, dx + x*texture_size, dy + z*texture_size);
				blocks.pop_back();
			}
		}
	}
}

void TopdownTileRenderer::renderTile(const TilePos& tile_pos, RGBAImage& tile) {
	int texture_size = images->getTextureSize();
	tile.setSize(getTileSize(), getTileSize());

	for (int x = 0; x < tile_width; x++) {
		for (int z = 0; z < tile_width; z++) {
			mc::ChunkPos chunkpos(tile_pos.getX() * tile_width + x, tile_pos.getY() * tile_width + z);
			current_chunk = world->getChunk(chunkpos);
			if (current_chunk != nullptr)
				renderChunk(*current_chunk, tile, texture_size*16*x, texture_size*16*z);
		}
	}
}

int TopdownTileRenderer::getTileSize() const {
	return images->getBlockSize() * 16 * tile_width;
}

}
}
