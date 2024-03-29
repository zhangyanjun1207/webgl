#if 0  // A cute trick to making this .cc self-building from shell.
g++ $0 -O2 -Wall -Werror -o `basename $0 .cc`;
exit;
#endif
// Copyright 2011 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you
// may not use this file except in compliance with the License. You
// may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.

#include "bounds.h"
#include "compress.h"
#include "mesh.h"
#include "optimize.h"
#include "stream.h"

int main() {
	//const char* []argv = {"","hand_00.obj","hand.utf8","hand.json"};
	FILE* json_out = stdout;
   json_out = fopen("/home/zhang/workspace/test001/src/hand.json", "w");
      CHECK(json_out != NULL);



  FILE* fp = fopen("/home/zhang/workspace/test001/src/hand.obj", "r");
  WavefrontObjFile obj(fp);
  fclose(fp);

  fputs("{\n  \"materials\": {\n", json_out);
  const MaterialList& materials = obj.materials();
  for (size_t i = 0; i < materials.size(); ++i) {
    materials[i].DumpJson(json_out);
    const bool last = i == materials.size() - 1;
    fputs(",\n" + last, json_out);
  }
  fputs("  },\n", json_out);
  
  const MaterialBatches& batches = obj.material_batches();

  // Pass 1: compute bounds.
  webgl_loader::Bounds bounds;
  bounds.Clear();
  for (MaterialBatches::const_iterator iter = batches.begin();
       iter != batches.end(); ++iter) {
    const DrawBatch& draw_batch = iter->second;
    bounds.Enclose(draw_batch.draw_mesh().attribs);
  }
  webgl_loader::BoundsParams bounds_params = 
      webgl_loader::BoundsParams::FromBounds(bounds);
  fputs("  \"decodeParams\": ", json_out);
  bounds_params.DumpJson(json_out);
  fputs(", \"urls\": {\n", json_out);
  // Pass 2: quantize, optimize, compress, report.
  FILE* utf8_out_fp = fopen("/home/zhang/workspace/test001/src/hand.utf8", "wb");
  CHECK(utf8_out_fp != NULL);
  fprintf(json_out, "    \"%s\": [\n", "/home/zhang/workspace/test001/src/hand.utf8");
  webgl_loader::FileSink utf8_sink(utf8_out_fp);
  size_t offset = 0;
  MaterialBatches::const_iterator iter = batches.begin();
  while (iter != batches.end()) {
    const DrawMesh& draw_mesh = iter->second.draw_mesh();
    if (draw_mesh.indices.empty()) {
      ++iter;
      continue;
    }
    QuantizedAttribList quantized_attribs;
    webgl_loader::AttribsToQuantizedAttribs(draw_mesh.attribs, bounds_params,
					    &quantized_attribs);
    VertexOptimizer vertex_optimizer(quantized_attribs);
    const std::vector<GroupStart>& group_starts = iter->second.group_starts();
    WebGLMeshList webgl_meshes;
    std::vector<size_t> group_lengths;
    for (size_t i = 1; i < group_starts.size(); ++i) {
      const size_t here = group_starts[i-1].offset;
      const size_t length = group_starts[i].offset - here;
      group_lengths.push_back(length);
      vertex_optimizer.AddTriangles(&draw_mesh.indices[here], length,
                                    &webgl_meshes);
    }
    const size_t here = group_starts.back().offset;
    const size_t length = draw_mesh.indices.size() - here;
    CHECK(length % 3 == 0);
    group_lengths.push_back(length);
    vertex_optimizer.AddTriangles(&draw_mesh.indices[here], length,
                                  &webgl_meshes);

    std::vector<std::string> material;
    std::vector<size_t> attrib_start, attrib_length, index_start, index_length;
    for (size_t i = 0; i < webgl_meshes.size(); ++i) {
      const size_t num_attribs = webgl_meshes[i].attribs.size();
      const size_t num_indices = webgl_meshes[i].indices.size();
      CHECK(num_attribs % 8 == 0);
      CHECK(num_indices % 3 == 0);
      webgl_loader::CompressQuantizedAttribsToUtf8(webgl_meshes[i].attribs, 
						   &utf8_sink);
      webgl_loader::CompressIndicesToUtf8(webgl_meshes[i].indices, &utf8_sink);
      material.push_back(iter->first);
      attrib_start.push_back(offset);
      attrib_length.push_back(num_attribs / 8);
      index_start.push_back(offset + num_attribs);
      index_length.push_back(num_indices / 3);
      offset += num_attribs + num_indices;
    }
    for (size_t i = 0; i < webgl_meshes.size(); ++i) {
      fprintf(json_out,
              "      { \"material\": \"%s\",\n"
              "        \"attribRange\": [" PRIuS ", " PRIuS "],\n"
              "        \"indexRange\": [" PRIuS ", " PRIuS "]\n"
              "      }",
              material[i].c_str(),
              attrib_start[i], attrib_length[i],
              index_start[i], index_length[i]);
      if (i != webgl_meshes.size() - 1) {
        fputs(",\n", json_out);
      }
    }
    const bool last = (++iter == batches.end());
    fputs(",\n" + last, json_out);
  }
  fputs("    ]\n", json_out);
  fputs("  }\n}", json_out);
  return 0;
}
