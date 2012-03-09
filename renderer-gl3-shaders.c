/* included by renderer-gl3.c -- do not compile separately */

#define DEF_BLOCK_SCENE \
"layout(std140) uniform scene {\n"\
"  uniform mat4 viewmatrix;\n"\
"  uniform mat4 projmatrix;\n"\
"  uniform mat4 iprojmatrix;\n"\
"  uniform mat4 vpmatrix;\n"\
"  uniform vec3 viewpos;\n"\
"  uniform mat3 viewrot;\n"\
"  uniform vec4 ray_eye[4];\n"\
"  uniform vec4 ray_world[4];\n"\
"  uniform vec4 viewport;\n"\
"  uniform float near;\n"\
"  uniform float far;\n"\
"};\n"

#define DEF_NORMAL_ENCODING \
"vec2 encode_normal(vec3 n) {\n"\
"  n = normalize(n);\n"\
"  float scale = sqrt(2.0/(1.0 + n.z));\n"\
"  return n.xy * scale * 0.25 + 0.5;\n"\
"}\n"\
"vec3 decode_normal(vec2 xy) {\n"\
"  xy = xy * 4.0 - 2.0;\n"\
"  float lensq = dot(xy, xy);\n"\
"  float xyscale = sqrt(1.0 - lensq / 4.0);\n"\
"  float zscale  = lensq / 2.0 - 1.0;\n"\
"  return normalize(vec3(xy * xyscale, -zscale));\n"\
"}\n"

static const char *vshader_gbuffer_src =
"#version 330\n"
DEF_BLOCK_SCENE
"layout(location = 0) in vec3 vposition;\n"
"layout(location = 1) in vec2 vtexcoord;\n"
"layout(location = 2) in vec3 vnormal;\n"
"layout(location = 3) in vec4 vtangent;\n"
"smooth out float fdepth;\n"
"smooth out vec2 ftexcoord;\n"
"smooth out mat3 tbnmatrix;\n"
"void main() {\n"
"  fdepth    = -(viewmatrix * vec4(vposition, 1.0)).z;\n"
"  ftexcoord = vtexcoord;\n"
"\n"
"  vec3 vbitangent = cross(vnormal, vtangent.xyz) * vtangent.w;\n"
"  tbnmatrix = viewrot * mat3(vtangent.xyz, vbitangent, vnormal);\n"
"\n"
"  gl_Position = vpmatrix * vec4(vposition, 1.0);\n"
"}\n";

static const char *fshader_gbuffer_src =
"#version 330\n"
DEF_BLOCK_SCENE
"uniform sampler2D tdiffuse;\n"
"uniform sampler2D tnormal;\n"
"uniform sampler2D tspecular;\n"
"uniform sampler2D temissive;\n"
"smooth in float fdepth;\n"
"smooth in vec2  ftexcoord;\n"
"smooth in mat3  tbnmatrix;\n"
"layout(location = 0) out float gdepth;\n"
"layout(location = 1) out vec2  gnormal;\n"
"layout(location = 2) out vec4  gdiffuse;\n"
"layout(location = 3) out vec4  gspecular;\n"
"layout(location = 4) out vec4  gemissive;\n"
DEF_NORMAL_ENCODING
"void main() {\n"
"  vec4 diffuse = texture(tdiffuse, ftexcoord);\n"
"  if (diffuse.a < 0.5) discard;\n"
"\n"
"  vec3 n_tangent = texture(tnormal, ftexcoord).xyz * 2.0 - 1.0;\n"
"  vec3 n_eye = normalize(tbnmatrix * n_tangent);\n"
"\n"
"  gdepth    = fdepth;\n"
"  gdiffuse  = diffuse;\n"
"  gnormal   = encode_normal(n_eye);\n"
"  vec4 spec = texture(tspecular, ftexcoord);\n"
"  gspecular = vec4(spec.r, spec.a, 0.25, 0.0);\n"
"  gemissive = texture(temissive, ftexcoord);\n"
"}\n";

static const char *vshader_gbufferback_src =
"#version 330\n"
DEF_BLOCK_SCENE
"layout(location = 0) in vec3 vposition;\n"
"layout(location = 1) in vec2 vtexcoord;\n"
"layout(location = 2) in vec3 vnormal;\n"
"layout(location = 3) in vec4 vtangent;\n"
"smooth out float fdepth;\n"
"smooth out vec2 ftexcoord;\n"
"void main() {\n"
"  ftexcoord = vtexcoord;\n"
"  fdepth    = -(viewmatrix * vec4(vposition, 1.0)).z;\n"
"  gl_Position = vpmatrix * vec4(vposition, 1.0);\n"
"}\n";

static const char *fshader_gbufferback_src =
"#version 330\n"
"uniform sampler2D tdiffuse;\n"
"smooth in float fdepth;\n"
"smooth in vec2  ftexcoord;\n"
"layout(location = 0) out float gback;\n"
"void main() {\n"
"  if (texture(tdiffuse, ftexcoord).a < 0.5) discard;\n"
"  gback = fdepth;\n"
"}\n";

static const char *vshader_downsample_src =
"#version 330\n"
"layout(location = 0) in vec2 vcoord;\n"
"void main () {\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_downsample_src =
"#version 330\n"
"uniform sampler2DRect tdepth;\n"
"uniform sampler2DRect tback;\n"
"layout(location = 0) out float gdepth;\n"
"layout(location = 1) out float gback;\n"
"vec2 coord[] = vec2[](\n"
"  gl_FragCoord.xy * 2.0 + vec2(-0.5, -0.5),\n"
"  gl_FragCoord.xy * 2.0 + vec2( 0.5, -0.5),\n"
"  gl_FragCoord.xy * 2.0 + vec2(-0.5,  0.5),\n"
"  gl_FragCoord.xy * 2.0 + vec2( 0.5,  0.5)\n"
");\n"
"void main() {\n"
"  float depth_front = 0.0;\n"
"  float depth_back  = 0.0;\n"
"  for (int i=0; i < 4; i++) {\n"
"    depth_front += texture(tdepth, coord[i]).r;\n"
"    depth_back  += texture(tback,  coord[i]).r;\n"
"  }\n"
"  gdepth = depth_front / 4.0;\n"
"  gback  = depth_back  / 4.0;\n"
"}\n";

static const char *vshader_ssao_src = 
"#version 330\n"
"layout(location = 0) in vec2 vcoord;\n"
"layout(location = 1) in vec3 vray_eye;\n"
"layout(location = 2) in vec3 vray_world;\n"
"smooth out vec3 fray_eye;\n"
"void main () {\n"
"  fray_eye    = vray_eye;\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_ssao_src = 
"#version 330\n"
DEF_BLOCK_SCENE
"uniform sampler2DRect tdepth;\n"
"uniform sampler2DRect tback;\n"
"uniform sampler2D tnoise;\n"
"smooth in vec3 fray_eye;\n"
"layout(location = 0) out float ssao;\n"
DEF_NORMAL_ENCODING
"float calc_ao(vec3 offset, vec3 normal, float sigma) {\n"
"  vec3  dir  = normalize(offset);\n"
"  float dist = length(offset);\n"
"  float costheta = dot(dir, normal);\n"
"  float falloff = 1.0 / (1.0 + dist / sigma);\n"
"  float ao = max(0.0, costheta);\n"
"  return ao * falloff;\n"
"}\n"
"void main() {\n"
"  float depth  = texture(tdepth, gl_FragCoord.xy).r;\n"
"  vec3 coord  = fray_eye * depth;\n"
"  vec3 tangent = dFdx(coord);\n"
"  vec3 bitangent = dFdy(coord);\n"
"  vec3 normal = normalize(cross(tangent, bitangent));\n"
""
"  float occlusion = 0.0;\n"
"  for (int i=0; i < 16; i++) {\n"
"    vec2  offset = 4.0 * normalize(texture(tnoise, (gl_FragCoord.xy + vec2(1 << (i / 4), 1 << (i % 4))) / 256.0).xyz * 2.0 - 1.0).xy;\n"
"    vec2  occluder_texcoord = gl_FragCoord.xy + offset;\n"
"    float occluder_fdepth = texture(tdepth, occluder_texcoord).r;\n"
"    float occluder_bdepth = texture(tback, occluder_texcoord).r;\n"
"    vec3  occluder_ray    = mix(\n"
"      mix(ray_eye[0].xyz, ray_eye[1].xyz, occluder_texcoord.x / viewport.z),\n"
"      mix(ray_eye[3].xyz, ray_eye[2].xyz, occluder_texcoord.x / viewport.z),\n"
"      occluder_texcoord.y / viewport.w);\n"
"    vec3 occluder_fcoord = occluder_fdepth * occluder_ray;\n"
"    vec3 occluder_bcoord = occluder_bdepth * occluder_ray;\n"
"    vec3 occluder_foffset = occluder_fcoord - coord;\n"
"    vec3 occluder_boffset = occluder_bcoord - coord;\n"
"    occlusion += calc_ao(occluder_foffset, normal, 16.0);\n"
"    occlusion += calc_ao(occluder_boffset, normal, 16.0);\n"
"  };\n"
"  ssao = occlusion / 32.0;\n"
"}\n";

static const char *vshader_bilateral_src = 
"#version 330\n"
"layout(location = 0) in vec2 vcoord;\n"
"void main () {\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_bilateral_src =
"#version 330\n"
"uniform sampler2DRect thdepth;\n" /* high-rez depth */
"uniform sampler2DRect tldepth;\n" /* low-rez depth */
"uniform sampler2DRect timage;\n"
"layout(location = 0) out vec4 color;\n"
"float weight[] = float[](\n"
"  1.0, 2.0, 1.0,\n"
"  2.0, 4.0, 2.0,\n"
"  1.0, 2.0, 1.0\n"
");\n"
"vec2 offset[] = vec2[](\n"
"  vec2(-1, -1), vec2( 0, -1), vec2( 1, -1),\n"
"  vec2(-1,  0), vec2( 0,  0), vec2( 1,  0),\n"
"  vec2(-1,  1), vec2( 0,  1), vec2( 1,  1)\n"
");\n"
"void main() {\n"
"  vec2  ratio = textureSize(thdepth) / textureSize(tldepth);\n"
"  float depth = texture(thdepth, gl_FragCoord.xy).r;\n"
"  float total = 0.0;\n"
"  vec4  value = vec4(0.0, 0.0, 0.0, 0.0);\n"
"  for (int i=0; i < 9; i++) {\n"
"      vec2 texcoord = gl_FragCoord.xy / ratio + offset[i];\n"
"      float sample_depth = texture(tldepth, texcoord).r;\n"
"      float scale = abs(sample_depth - depth) > 16.0 ? 0.0 : 1.0;\n"
"      value += scale * weight[i] * texture(timage, texcoord);\n"
"      total += scale * weight[i];\n"
"  }\n"
"  color = total < 0.01 ? vec4(0.0, 0.0, 0.0, 0.0) : value / total;\n"
"}\n";

static const char *vshader_minimal_src = 
"#version 330\n"
DEF_BLOCK_SCENE
"uniform mat4 modelmatrix;\n"
"layout(location = 0) in vec3 vcoord;\n"
"void main () {\n"
"  gl_Position  = vpmatrix * modelmatrix * vec4(vcoord, 1.0);\n"
"}\n";

static const char *fshader_flatcolor_src = 
"#version 330\n"
"uniform vec3 color;\n"
"layout(location = 0) out vec4 outcolor;\n"
"void main() {\n"
"  outcolor = vec4(color, 1.0);\n"
"}\n";

static const char *vshader_pointlight_src = 
"#version 330\n"
DEF_BLOCK_SCENE
"layout(std140) uniform pointlight {\n"
"  vec4  position;\n"
"  vec4  color;\n"
"} light;\n"
"layout(location = 0) in vec2 vcoord;\n"
"layout(location = 1) in vec3 vray_eye;\n"
"layout(location = 2) in vec3 vray_world;\n"
"smooth out vec3 fray_eye;\n"
"smooth out vec3 fray_world;\n"
"smooth out vec3 fcenter_eye;\n"
"smooth out vec3 fcenter_world;\n"
"void main () {\n"
"  fray_eye      = vray_eye;\n"
"  fray_world    = vray_world;\n"
"  fcenter_eye   = (viewmatrix * light.position).xyz;\n"
"  fcenter_world = light.position.xyz;\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_pointlight_src =
"#version 330\n"
DEF_BLOCK_SCENE
"layout(std140) uniform pointlight {\n"
"  vec4  position;\n"
"  vec4  color;\n"
"} light;\n"
"uniform sampler2DRect tdepth;\n"
"uniform sampler2DRect tnormal;\n"
"uniform sampler2DRect tdiffuse;\n"
"uniform sampler2DRect tspecular;\n"
"uniform samplerCube   tshadow;\n"
"smooth in vec3 fray_eye;\n"
"smooth in vec3 fray_world;\n"
"smooth in vec3 fcenter_eye;\n"
"smooth in vec3 fcenter_world;\n"
"layout(location = 0) out vec4 color;\n"
DEF_NORMAL_ENCODING
"float Linfdist(vec3 a, vec3 b) {;\n" /* distance in L-infinity space */
"  float dist = 0.0f;\n"
"  dist = max(dist, abs(a.x - b.x));\n"
"  dist = max(dist, abs(a.y - b.y));\n"
"  dist = max(dist, abs(a.z - b.z));\n"
"  return dist;\n"
"}\n"
"void main () {\n"
"  float depth = texture(tdepth, gl_FragCoord.xy).r;\n"
"  vec3  coord_eye   = fray_eye * depth;\n"
"  vec3  coord_world = fray_world * depth + viewpos;\n"
"  float dist      = distance(fcenter_eye, coord_eye);\n"
"  float luminance = light.color.a / (dist * dist);\n"
""
"  vec3 N = decode_normal(texture(tnormal, gl_FragCoord.xy).xy);\n"
""
"  vec3 L = normalize(fcenter_eye - coord_eye);\n"
"  vec3 V = -normalize(fray_eye);\n"
"  vec3 H = normalize(L + V);\n"
"  vec3 R  = 2.0 * N * dot(N, L) - L;\n"
""
"  vec3 diff  = texture(tdiffuse, gl_FragCoord.xy).rgb;\n"
"  vec4 spec  = texture(tspecular, gl_FragCoord.xy);\n"
"  float fresnel_0 = spec.b;\n"
"  float fresnel = fresnel_0 + (1.0 - fresnel_0) * pow(1.0 - dot(V, H), 5.0);\n" /* fresnel term */
""
"  const float epsilon = 1.0;\n"
"  float shadow = dist - epsilon < texture(tshadow, coord_world - fcenter_world).r ? 1.0 : 0.0;\n"
""
"  color.rgb  = diff * max(0.0, dot(N, L));\n" /* diffuse */
"  vec3  spec_intensity = spec.rrr;\n"
"  float spec_exponent  = spec.g * 255.0;\n"
"  color.rgb += spec_intensity * fresnel * pow(max(0.0, dot(R, V)), spec_exponent);\n" /* specular */
"  color.rgb *= light.color.rgb * luminance * shadow;\n"
"  color.a    = 1.0;\n"
"}\n";

static const char *vshader_envlight_src = 
"#version 330\n"
"layout(location = 0) in vec2 vcoord;\n"
"layout(location = 1) in vec3 vray_eye;\n"
"layout(location = 2) in vec3 vray_world;\n"
"smooth out vec3 fray_eye;\n"
"smooth out vec3 fray_world;\n"
"void main () {\n"
"  fray_eye    = vray_eye;\n"
"  fray_world  = vray_world;\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_envlight_src =
"#version 330\n"
DEF_BLOCK_SCENE
"layout(std140) uniform envlight {\n"
"  vec4  ambient;\n"
"  vec4  color;\n"
"  vec4  direction;\n"
"} light;\n"
"uniform sampler2DRect tdepth;\n"
"uniform sampler2DRect tnormal;\n"
"uniform sampler2DRect tdiffuse;\n"
"uniform sampler2DRect tspecular;\n"
"uniform sampler2DRect temissive;\n"
"uniform sampler2DRect tocclusion;\n"
"smooth in vec3 fray_eye;\n"
"smooth in vec3 fray_world;\n"
"smooth in vec2 ftexcoord;\n"
"layout(location = 0) out vec4 color;\n"
DEF_NORMAL_ENCODING
"void main () {\n"
"  float depth = texture(tdepth, gl_FragCoord.xy).r;\n"
"  vec3  coord_eye = fray_eye * depth;\n"
"  vec3  coord_world = fray_world * depth + viewpos;\n"
""
"  vec3 N = decode_normal(texture(tnormal, gl_FragCoord.xy).xy);\n"
""
"  float occlusion = pow(1.0 - max(0.0, texture(tocclusion, gl_FragCoord.xy).r), 2.0);\n"
""
"  vec3 L = normalize(viewrot * -light.direction.xyz);\n"
"  vec3 V = -normalize(fray_eye);\n"
"  vec3 H = normalize(L + V);\n"
"  vec3 R = 2.0 * N * dot(N, L) - L;\n"
""
"  vec3 diff  = texture(tdiffuse, gl_FragCoord.xy).rgb;\n"
"  vec4 spec  = texture(tspecular, gl_FragCoord.xy);\n"
"  vec4 glow  = texture(temissive, gl_FragCoord.xy);\n"
""
"  float fresnel_0 = spec.b;\n"
"  float fresnel = fresnel_0 + (1.0 - fresnel_0) * pow(1.0 - dot(V, H), 5.0);\n" /* fresnel term */
""
"  color.rgb += diff * max(0.0, dot(N, L));\n" /* diffuse */
"  vec3  spec_intensity = spec.rrr;\n"
"  float spec_exponent  = spec.g * 255.0;\n"
"  color.rgb += spec_intensity * fresnel * pow(max(0.0, dot(R, V)), spec_exponent);\n" /* specular */
"  color.rgb *= light.color.rgb * light.color.a;\n" /* diffuse/specular scale */
"  color.rgb += diff * light.ambient.rgb * light.ambient.a * occlusion;\n" /* ambient */
"  color.rgb += glow.rgb * exp2(glow.a * 16.0 - 8.0);\n" /* emissive */
"  color.a    = 1.0;\n"
"}\n";

static const char *vshader_pointshadow_src =
"#version 330\n"
"uniform vec3 center;\n"
"uniform mat4 cubeproj[6];\n"
"uniform int  face;\n"
"layout(location = 0) in vec3 vposition;\n"
"layout(location = 1) in vec2 vtexcoord;\n"
"layout(location = 2) in vec3 vnormal;\n"
"layout(location = 3) in vec4 vtangent;\n"
"smooth out float fdist;\n"
"smooth out vec2 ftexcoord;\n"
"void main() {\n"
"  ftexcoord = vtexcoord;\n"
"  fdist = distance(vposition, center);\n"
"  gl_Position = cubeproj[face] * vec4(vposition - center, 1.0);\n"
"}\n";

static const char *fshader_pointshadow_src = 
"#version 330\n"
"uniform sampler2D tdiffuse;\n"
"smooth in float fdist;\n"
"smooth in vec2 ftexcoord;\n"
"layout(location = 0) out float dist;\n"
"void main () {\n"
"  float alpha = texture(tdiffuse, ftexcoord).a;\n"
"  if (alpha < 0.5) discard;\n"
"  dist = fdist;\n"
"}\n";

static const char *vshader_tonemap_src = 
"#version 330\n"
"layout(location = 0) in vec2 vcoord;\n"
"smooth out vec2 ftexcoord;\n"
"void main () {\n"
"  ftexcoord   = (vcoord * 0.5 + 0.5);\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_tonemap_src = 
"#version 330\n"
"uniform sampler2D tcomposite;\n"
"uniform float sigma;\n"
"smooth in vec2 ftexcoord;\n"
"layout(location = 0) out vec4 color;\n"
"vec3 tonemap(vec3 color, float sigma) {\n"
"  color     = max(vec3(0.0, 0.0, 0.0), color);\n"
"  vec3  c2  = color * color;\n"
"  float var = sigma * sigma;\n"
"  return c2/(c2+var);\n"
"}\n"
"void main() {\n"
"  vec3 c = texture(tcomposite, ftexcoord).rgb;\n"
"  color = vec4(tonemap(c, sigma), 1.0);\n"
"}\n";

static const char *vshader_blit_src =
"#version 330\n"
"uniform vec2 size;\n"
"uniform vec2 offset;\n"
"layout(location = 0) in vec2 vcoord;\n"
"smooth out vec2 ftexcoord;\n"
"void main () {\n"
"  ftexcoord   = (vcoord * 0.5 + 0.5);\n"
"  gl_Position = vec4(vcoord * size + offset, 0.0, 1.0);\n"
"}\n";

static const char *fshader_blit_src = 
"#version 330\n"
"uniform float colorscale;\n"
"uniform float coloroffset;\n"
"uniform sampler2DRect image;\n"
"smooth in vec2 ftexcoord;\n"
"layout(location = 0) out vec4 color;\n"
"void main() {\n"
"  color = texture(image, ftexcoord * textureSize(image)) * colorscale + coloroffset;\n"
"}\n";

static const char *fshader_blit_cube_src = 
"#version 330\n"
DEF_NORMAL_ENCODING
"uniform float colorscale;\n"
"uniform float coloroffset;\n"
"uniform samplerCube image;\n"
"smooth in vec2 ftexcoord;\n"
"layout(location = 0) out vec4 color;\n"
"void main() {\n"
"  float r = distance(ftexcoord, vec2(0.5, 0.5));\n"
"  if (r > 0.5) discard;\n"
"  color.rgb = texture(image, decode_normal(ftexcoord)).rgb * colorscale + coloroffset;\n"
"  color.a   = (0.5 - r) / 0.025;\n"
"}\n";