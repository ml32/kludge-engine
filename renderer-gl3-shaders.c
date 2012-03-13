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
"uniform sampler2DRect tnormal;\n"
"layout(location = 0) out float gdepth;\n"
"layout(location = 1) out float gback;\n"
"layout(location = 2) out vec2 gnormal;\n"
"vec2 coord[] = vec2[](\n"
"  gl_FragCoord.xy * 2.0 + vec2(-0.5, -0.5),\n"
"  gl_FragCoord.xy * 2.0 + vec2( 0.5, -0.5),\n"
"  gl_FragCoord.xy * 2.0 + vec2(-0.5,  0.5),\n"
"  gl_FragCoord.xy * 2.0 + vec2( 0.5,  0.5)\n"
");\n"
DEF_NORMAL_ENCODING
"void main() {\n"
"  float depth_front = 0.0;\n"
"  float depth_back  = 0.0;\n"
"  vec3  normal = vec3(0.0, 0.0, 0.0);\n"
"  for (int i=0; i < 4; i++) {\n"
"    depth_front += texture(tdepth, coord[i]).r;\n"
"    depth_back  += texture(tback,  coord[i]).r;\n"
"    normal += decode_normal(texture(tnormal, coord[i]).xy);\n"
"  }\n"
"  gdepth = depth_front / 4.0;\n"
"  gback  = depth_back  / 4.0;\n"
"  gnormal = encode_normal(normalize(normal));\n"
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
"vec2 poisson[] = vec2[](\n"
"  vec2( 0.3085, -0.1495),\n"
"  vec2( 0.5075,  0.4990),\n"
"  vec2( 0.4927, -0.7529),\n"
"  vec2( 0.7344, -0.0500),\n"
"  vec2( 0.0734,  0.4966),\n"
"  vec2(-0.2320, -0.5536),\n"
"  vec2(-0.2422,  0.2106),\n"
"  vec2(-0.6254, -0.4917),\n"
"  vec2(-0.7222, -0.0555),\n"
"  vec2( 0.0966, -0.9465),\n"
"  vec2( 0.1808, -0.5089),\n"
"  vec2( 0.1146,  0.9233),\n"
"  vec2(-0.4045,  0.7184),\n"
"  vec2(-0.7903,  0.5653),\n"
"  vec2( 0.7250, -0.4404),\n"
"  vec2(-0.2766, -0.1738)\n"
");\n"
"void main() {\n"
"  float depth  = texture(tdepth, gl_FragCoord.xy).r;\n"
"  vec3 coord  = fray_eye * depth;\n"
"  vec3 tangent = dFdx(coord);\n"
"  vec3 bitangent = dFdy(coord);\n"
"  vec3 normal = normalize(cross(tangent, bitangent));\n"
""
"  float occlusion = 0.0;\n"
"  for (int i=0; i < 16; i++) {\n"
"    vec2  offset = 8.0 * poisson[i];\n"
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
"    occlusion += calc_ao(occluder_foffset, normal, 64.0);\n"
"    occlusion += calc_ao(occluder_boffset, normal, 64.0);\n"
"  };\n"
"  ssao = occlusion / 48.0;\n"
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
/*
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
*/
"float weight[] = float[](\n"
"  1.0,  4.0,  7.0,  4.0, 1.0,\n"
"  4.0, 16.0, 26.0, 16.0, 4.0,\n"
"  7.0, 26.0, 41.0, 26.0, 7.0,\n"
"  4.0, 16.0, 26.0, 16.0, 4.0,\n"
"  1.0,  4.0,  7.0,  4.0, 1.0\n"
");\n"
"float div = 277.0;\n"
"vec2 offset[] = vec2[](\n"
"  vec2(-2, -2), vec2(-1, -2), vec2( 0, -2), vec2( 1, -2), vec2( 2, -2),\n"
"  vec2(-2, -1), vec2(-1, -1), vec2( 0, -1), vec2( 1, -1), vec2( 2, -1),\n"
"  vec2(-2,  0), vec2(-1,  0), vec2( 0,  0), vec2( 1,  0), vec2( 2,  0),\n"
"  vec2(-2,  1), vec2(-1,  1), vec2( 0,  1), vec2( 1,  1), vec2( 2,  1),\n"
"  vec2(-2,  2), vec2(-1,  2), vec2( 0,  2), vec2( 1,  2), vec2( 2,  2)\n"
");\n"
"void main() {\n"
"  vec2  ratio = textureSize(thdepth) / textureSize(tldepth);\n"
"  float depth = texture(thdepth, gl_FragCoord.xy).r;\n"
"  float total = 0.0;\n"
"  vec4  value = vec4(0.0, 0.0, 0.0, 0.0);\n"
"  vec4  fallback = vec4(0.0, 0.0, 0.0, 0.0);\n"
"  for (int i=0; i < 25; i++) {\n"
"      vec2 texcoord = gl_FragCoord.xy / ratio + offset[i];\n"
"      float sample_depth = texture(tldepth, texcoord).r;\n"
"      float scale = abs(sample_depth - depth) > 0.01 * depth ? 0.0 : 1.0;\n"
"      vec4 sample = weight[i] * texture(timage, texcoord);\n"
"      fallback += sample;\n"
"      value += scale * sample;\n"
"      total += scale * weight[i];\n"
"  }\n"
""
"  color = total < 0.001 ? fallback / div : value / total;\n"
"}\n";

static const char *vshader_shadowfilter_src = 
"#version 330\n"
"layout(location = 0) in vec2 vcoord;\n"
"layout(location = 1) in vec3 vray_eye;\n"
"layout(location = 2) in vec3 vray_world;\n"
"smooth out vec3 fray_eye;\n"
"void main () {\n"
"  fray_eye = vray_eye;\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_shadowfilter_src =
"#version 330\n"
"uniform sampler2DRect tdepth;\n"
"uniform sampler2DRect tnormal;\n"
"uniform sampler2DRect tshadow;\n"
"uniform int pass;\n"
"smooth in vec3 fray_eye;\n"
"layout(location = 0) out float shadow;\n"
"float weights[] = float[](0.2369, 0.5273, 0.8521, 1.0000, 0.8521, 0.5273, 0.2369);\n"
DEF_NORMAL_ENCODING
"void main() {\n"
"  float depth = texture(tdepth, gl_FragCoord.xy).r;\n"
"  vec3 N = decode_normal(texture(tnormal, gl_FragCoord.xy).xy);\n"
"  vec3 T = normalize(cross(N, vec3(0.0, 0.0, -1.0)));\n"
"  vec3 B = normalize(cross(N, T));\n"
"  vec2 axis = pass == 0 ? normalize(T.xy) : B.xy;\n"
"  float total = 0.0;\n"
"  float value = 0.0;\n"
"  for (int i=0; i < 7; i++) {\n"
"      float scale = min(2000.0 / depth, 3.0);\n"
"      vec2 offset = scale * axis * (i - 3);\n"
"      vec2 texcoord = gl_FragCoord.xy + offset;\n"
"      float sample_depth = texture(tdepth, texcoord).r;\n"
""
"      float cutoff = abs(sample_depth - depth) > 3.0 ? 0.0 : 1.0;\n"
"      float weight = weights[i];\n"
"      value += cutoff * weight * texture(tshadow, texcoord).r;\n"
"      total += cutoff * weight;\n"
"  }\n"
"  shadow = total < 0.01 ? 0.0 : value / total;\n"
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

static const char *vshader_lighting_src = 
"#version 330\n"
"layout(location = 0) in vec3 vcoord;\n"
"void main () {\n"
"  gl_Position = vec4(vcoord, 1.0);\n"
"}\n";

static const char *fshader_lighting_src = 
"#version 330\n"
"uniform sampler2DRect tdiffuse;\n"
"uniform sampler2DRect tspecular;\n"
"uniform sampler2DRect tambient;\n"
"uniform sampler2DRect talbedo;\n"
"uniform sampler2DRect tocclusion;\n"
"uniform sampler2DRect temissive;\n"
"layout(location = 0) out vec4 final;\n"
"void main() {\n"
"  final.rgb  = texture(tambient, gl_FragCoord.xy).rgb;\n"
"  final.rgb *= 1.0 - texture(tocclusion, gl_FragCoord.xy).r;\n"
"  final.rgb += texture(tdiffuse, gl_FragCoord.xy).rgb;\n"
"  final.rgb *= texture(talbedo, gl_FragCoord.xy).rgb;\n"
"  final.rgb += texture(tspecular, gl_FragCoord.xy).rgb;\n"
"  vec4 glow  = texture(temissive, gl_FragCoord.xy);\n"
"  final.rgb += glow.rgb * exp2(glow.a * 64.0 - 32.0);\n"
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
"smooth out vec3 fcenter_eye;\n"
"void main () {\n"
"  fray_eye      = vray_eye;\n"
"  fcenter_eye   = (viewmatrix * light.position).xyz;\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_pointlight_src =
"#version 330\n"
"layout(std140) uniform pointlight {\n"
"  vec4  position;\n"
"  vec4  color;\n"
"} light;\n"
"uniform sampler2DRect tdepth;\n"
"uniform sampler2DRect tnormal;\n"
"uniform sampler2DRect tspecular;\n"
"uniform sampler2DRect tshadow;\n"
"smooth in vec3 fray_eye;\n"
"smooth in vec3 fray_world;\n"
"smooth in vec3 fcenter_eye;\n"
"layout(location = 0) out vec4 diffuse;\n"
"layout(location = 1) out vec4 specular;\n"
DEF_NORMAL_ENCODING
"void main () {\n"
"  float depth = texture(tdepth, gl_FragCoord.xy).r;\n"
"  vec3  coord_eye = fray_eye * depth;\n"
"  float dist = distance(fcenter_eye, coord_eye);\n"
"  vec3  irradiance = light.color.rgb * light.color.a / (dist * dist);\n"
""
"  vec3 N = decode_normal(texture(tnormal, gl_FragCoord.xy).xy);\n"
""
"  vec3 L = normalize(fcenter_eye - coord_eye);\n"
"  vec3 V = -normalize(fray_eye);\n"
"  vec3 H = normalize(L + V);\n"
"  vec3 R  = 2.0 * N * dot(N, L) - L;\n"
""
"  vec4 spec = texture(tspecular, gl_FragCoord.xy);\n"
"  float fresnel_0 = spec.b;\n"
"  float fresnel = fresnel_0 + (1.0 - fresnel_0) * pow(1.0 - dot(V, H), 5.0);\n" /* fresnel term */
""
"  const float epsilon = 1.0;\n"
"  float shadow = texture(tshadow, gl_FragCoord.xy).r;\n"
""
"  diffuse.rgb = max(0.0, dot(N, L)) * irradiance * shadow;\n"
"  diffuse.a   = 1.0;\n"
""
"  vec3  spec_intensity = spec.rrr;\n"
"  float spec_exponent  = spec.g * 255.0;\n"
"  specular.rgb = spec_intensity * fresnel * pow(max(0.0, dot(R, V)), spec_exponent) * irradiance * shadow;\n"
"  specular.a   = 1.0;\n"
"}\n";

static const char *vshader_surfacelight_src = 
"#version 330\n"
DEF_BLOCK_SCENE
"uniform vec3 position;\n"
"uniform vec3 direction;\n"
"layout(location = 0) in vec2 vcoord;\n"
"layout(location = 1) in vec3 vray_eye;\n"
"layout(location = 2) in vec3 vray_world;\n"
"smooth out vec3 fray_eye;\n"
"smooth out vec3 fcenter_eye;\n"
"smooth out vec3 fdir_eye;\n"
"void main () {\n"
"  fray_eye = vray_eye;\n"
"  fcenter_eye = (viewmatrix * vec4(position, 1.0)).xyz;\n"
"  fdir_eye = viewrot * direction;\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_surfacelight_src =
"#version 330\n"
"uniform sampler2DRect tdepth;\n"
"uniform sampler2DRect tnormal;\n"
"uniform vec3 radiosity;\n"
"smooth in vec3 fray_eye;\n"
"smooth in vec3 fcenter_eye;\n"
"smooth in vec3 fdir_eye;\n"
"layout(location = 0) out vec4 ambient;\n"
DEF_NORMAL_ENCODING
"void main () {\n"
"  float depth = texture(tdepth, gl_FragCoord.xy).r;\n"
"  vec3  coord_eye = fray_eye * depth;\n"
"  float dist = distance(fcenter_eye, coord_eye);\n"
"  float falloff = min(1.0 / (64.0 * 64.0), 1.0f / (dist * dist));\n"
""
"  vec3 N = decode_normal(texture(tnormal, gl_FragCoord.xy).xy);\n"
"  vec3 L = normalize(fcenter_eye - coord_eye);\n"
"  vec3 irradiance = radiosity * max(0.0, dot(fdir_eye, -L)) * falloff;\n"
""
"  ambient.rgb = max(0.0, dot(N, L)) * irradiance;\n"
//"  ambient.rgb = vec3(1.0, 1.0, 1.0);\n"
"  ambient.a   = 1.0;\n"
"}\n";

static const char *vshader_envlight_src = 
"#version 330\n"
"layout(location = 0) in vec2 vcoord;\n"
"layout(location = 1) in vec3 vray_eye;\n"
"layout(location = 2) in vec3 vray_world;\n"
"smooth out vec3 fray_eye;\n"
"void main () {\n"
"  fray_eye    = vray_eye;\n"
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
"uniform sampler2DRect tspecular;\n"
"smooth in vec3 fray_eye;\n"
"smooth in vec2 ftexcoord;\n"
"layout(location = 0) out vec4 diffuse;\n"
"layout(location = 1) out vec4 specular;\n"
"layout(location = 2) out vec4 ambient;\n"
DEF_NORMAL_ENCODING
"void main () {\n"
"  float depth = texture(tdepth, gl_FragCoord.xy).r;\n"
"  vec3  coord_eye = fray_eye * depth;\n"
""
"  vec3 N = decode_normal(texture(tnormal, gl_FragCoord.xy).xy);\n"
""
"  vec3 L = normalize(viewrot * -light.direction.xyz);\n"
"  vec3 V = -normalize(fray_eye);\n"
"  vec3 H = normalize(L + V);\n"
"  vec3 R = 2.0 * N * dot(N, L) - L;\n"
""
"  vec4 spec  = texture(tspecular, gl_FragCoord.xy);\n"
""
"  float fresnel_0 = spec.b;\n"
"  float fresnel = fresnel_0 + (1.0 - fresnel_0) * pow(1.0 - dot(V, H), 5.0);\n" /* fresnel term */
""
"  vec3  irradiance = light.color.rgb * light.color.a;\n"
""
"  diffuse.rgb = max(0.0, dot(N, L)) * irradiance;\n"
"  diffuse.a   = 1.0;\n"
"  vec3  spec_intensity = spec.rrr;\n"
"  float spec_exponent  = spec.g * 255.0;\n"
"  specular.rgb = spec_intensity * fresnel * pow(max(0.0, dot(R, V)), spec_exponent) * irradiance;\n"
"  specular.a   = 1.0;\n"
"  ambient.rgb = light.ambient.rgb * light.ambient.a;\n"
"  ambient.a   = 1.0;\n"
"}\n";

static const char *vshader_cubedepth_src =
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

static const char *fshader_cubedepth_src = 
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

static const char *vshader_pointbounce_src =
"#version 330\n"
"layout(std140) uniform pointlight {\n"
"  vec4  position;\n"
"  vec4  color;\n"
"} light;\n"
"uniform mat4 cubeproj[6];\n"
"uniform int  face;\n"
"layout(location = 0) in vec3 vposition;\n"
"layout(location = 1) in vec2 vtexcoord;\n"
"layout(location = 2) in vec3 vnormal;\n"
"layout(location = 3) in vec4 vtangent;\n"
"smooth out vec3 fposition;\n"
"smooth out vec2 ftexcoord;\n"
"smooth out mat3 tbnmatrix;\n"
"void main() {\n"
"  fposition = vposition;\n"
"  ftexcoord = vtexcoord;\n"
"\n"
"  vec3 vbitangent = cross(vnormal, vtangent.xyz) * vtangent.w;\n"
"  tbnmatrix = mat3(vtangent.xyz, vbitangent, vnormal);\n"
"\n"
"  gl_Position = cubeproj[face] * vec4(vposition - light.position.xyz, 1.0);\n"
"}\n";

static const char *fshader_pointbounce_src =
"#version 330\n"
"layout(std140) uniform pointlight {\n"
"  vec4  position;\n"
"  vec4  color;\n"
"} light;\n"
"uniform sampler2D tdiffuse;\n"
"uniform sampler2D tnormal;\n"
"smooth in vec3 fposition;\n"
"smooth in vec2 ftexcoord;\n"
"smooth in mat3 tbnmatrix;\n"
"layout(location = 0) out vec4 position;\n"
"layout(location = 1) out vec4 normal;\n"
"layout(location = 2) out vec4 radiosity;\n"
"void main() {\n"
"  vec4 diff = texture(tdiffuse, ftexcoord);\n"
"  if (diff.a < 0.5) discard;\n"
""
"  float dist = distance(fposition, light.position.xyz);\n"
//"  float falloff = 1.0 / (dist * dist);\n"
"  float falloff = 1.0;\n"
"  float irradiance = light.color.a * falloff;\n"
""
"  vec3 N_tangent = texture(tnormal, ftexcoord).xyz * 2.0 - 1.0;\n"
"  vec3 N = normalize(tbnmatrix * N_tangent);\n"
"  vec3 L = normalize(light.position.xyz - fposition);\n"
""
"  position  = vec4(fposition, 1.0);\n"
"  normal    = vec4(N, 1.0);\n"
//"  radiosity.rgb = diff.rgb * max(0.0, dot(N, L)) * light.color.rgb * irradiance;\n"
"  radiosity.rgb = diff.rgb * light.color.rgb * irradiance;\n"
"  radiosity.a   = falloff;\n"
"}\n";

static const char *vshader_pointshadow_src = 
"#version 330\n"
DEF_BLOCK_SCENE
"uniform vec3 center;\n"
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
"  fcenter_eye   = (viewmatrix * vec4(center, 1.0)).xyz;\n"
"  fcenter_world = center;\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_pointshadow_src =
"#version 330\n"
DEF_BLOCK_SCENE
"uniform sampler2DRect tscreendepth;\n"
"uniform samplerCube   tcubedepth;\n"
"smooth in vec3 fray_eye;\n"
"smooth in vec3 fray_world;\n"
"smooth in vec3 fcenter_eye;\n"
"smooth in vec3 fcenter_world;\n"
"layout(location = 0) out float shadow;\n"
"void main () {\n"
"  float depth = texture(tscreendepth, gl_FragCoord.xy).r;\n"
"  vec3  coord_eye   = fray_eye * depth;\n"
"  vec3  coord_world = fray_world * depth + viewpos;\n"
"  float dist = distance(fcenter_eye, coord_eye);\n"
""
"  const float epsilon = 1.0;\n"
"  shadow = dist - epsilon < texture(tcubedepth, coord_world - fcenter_world).r ? 1.0 : 0.0;\n"
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