module pspemu.core.gpu.ops.Gpu_Lights;

template Gpu_Lights() {
	string LightArrayOperation(string type, string code, int step = 1) { return ArrayOperation(type, 0, 3, code, step); }
	string LightArrayOperationStep3(string type, string code) { return LightArrayOperation(type, code, 3); }
	
	// Lighting Test Enable GL_LIGHTING.
	auto OP_LTE() { gpu.state.lighting.enabled = command.bool1; }
	
	// Ambient Light Color/Alpha
	auto OP_ALC() { gpu.state.lighting.ambientLightColor.rgb[] = command.float3[]; }
	auto OP_ALA() { gpu.state.lighting.ambientLightColor.alpha = command.float4[0]; }

	// LighT Enable (per light)
	mixin(LightArrayOperation("OP_LTE_n", q{ gpu.state.lighting.lights[Index].enabled = command.bool1; }));

	/**
	 * Set light parameters
	 *
	 * Available light types are:
	 *   - GU_DIRECTIONAL - Directional light
	 *   - GU_POINTLIGHT  - Single point of light
	 *   - GU_SPOTLIGHT   - Point-light with a cone
	 *
	 * Available light components are:
	 *   - GU_AMBIENT_AND_DIFFUSE
	 *   - GU_DIFFUSE_AND_SPECULAR
	 *   - GU_UNKNOWN_LIGHT_COMPONENT
	 *
	 * @param light      - Light index
	 * @param type       - Light type
	 * @param components - Light components
	 * @param position   - Light position
	 **/
	// void sceGuLight(int light, int type, int components, const ScePspFVector3* position); // OP_LXP_n + OP_LYP_n + OP_LZP_n + OP_LT_n

	// Light Position (X, Y, Z) (per light)
	mixin(LightArrayOperationStep3("OP_LXP_n", q{ gpu.state.lighting.lights[Index].position.x = command.float1; }));
	mixin(LightArrayOperationStep3("OP_LYP_n", q{ gpu.state.lighting.lights[Index].position.y = command.float1; }));
	mixin(LightArrayOperationStep3("OP_LZP_n", q{ gpu.state.lighting.lights[Index].position.z = command.float1; }));

	// Light Type (per light)
	mixin(LightArrayOperation("OP_LT_n" , q{
		with (gpu.state.lighting.lights[Index]) {
			type = command.extractEnum!(LightType, 8);
			kind = command.extractEnum!(LightModel, 0);
			switch (type) {
				case LightType.GU_DIRECTIONAL:
					position.t = 0.0;
				break;
				case LightType.GU_POINTLIGHT:
					position.t = 1.0;
					spotCutoff = 180;
				break;
				case LightType.GU_SPOTLIGHT:
					position.t = 1.0;
				break;
				default:
					throw(new Exception("Unexpected LightType"));
			}
		}
	}));

	/**
	 * Set light mode
	 *
	 * Available light modes are:
	 *   - GU_SINGLE_COLOR
	 *   - GU_SEPARATE_SPECULAR_COLOR
	 *
	 * Separate specular colors are used to interpolate the specular component
	 * independently, so that it can be added to the fragment after the texture color.
	 *
	 * @param mode - Light mode to use
	 **/
	// void sceGuLightMode(int mode);
	// Light MODE (global)
	auto OP_LMODE() { gpu.state.lighting.lightModel = command.extractEnum!(LightModel); }

	/**
	 * Set light attenuation
	 *
	 * @param light  - Light index
	 * @param atten0 - Constant attenuation factor
	 * @param atten1 - Linear attenuation factor
	 * @param atten2 - Quadratic attenuation factor
	 **/
	// void sceGuLightAtt(int light, float atten0, float atten1, float atten2); // OP_LCA_n + OP_LLA_n + OP_LQA_n
	// Light Constant/Linear/Quadratic Attenuation (per light)
	mixin(LightArrayOperationStep3("OP_LCA_n", q{ gpu.state.lighting.lights[Index].attenuation.constant  = command.float1; }));
	mixin(LightArrayOperationStep3("OP_LLA_n", q{ gpu.state.lighting.lights[Index].attenuation.linear    = command.float1; }));
	mixin(LightArrayOperationStep3("OP_LQA_n", q{ gpu.state.lighting.lights[Index].attenuation.quadratic = command.float1; }));

	/**
	 * Set spotlight parameters
	 *
	 * @param light     - Light index
	 * @param direction - Spotlight direction
	 * @param exponent  - Spotlight exponent
	 * @param cutoff    - Spotlight cutoff angle (in radians)
	 **/
	// void sceGuLightSpot(int light, const ScePspFVector3* direction, float exponent, float cutoff); // OP_SPOTEXP_n + OP_SPOTCUT_n + OP_LXD_n + OP_LYD_n + OP_LZD_n

	// spot Light Direction (X, Y, Z) (per light)
	mixin(LightArrayOperationStep3("OP_LXD_n", q{ gpu.state.lighting.lights[Index].spotDirection.x = command.float1; }));
	mixin(LightArrayOperationStep3("OP_LYD_n", q{ gpu.state.lighting.lights[Index].spotDirection.y = command.float1; }));
	mixin(LightArrayOperationStep3("OP_LZD_n", q{ gpu.state.lighting.lights[Index].spotDirection.z = command.float1; }));
	// SPOT light EXPonent/CUToff (per light)
	mixin(LightArrayOperation("OP_SPOTEXP_n", q{ gpu.state.lighting.lights[Index].spotExponent = command.float1; }));
	mixin(LightArrayOperation("OP_SPOTCUT_n", q{ gpu.state.lighting.lights[Index].spotCutoff   = command.float1; }));

	/**
	 * Set light color
	 *
	 * Available light components are:
	 *   - GU_AMBIENT
	 *   - GU_DIFFUSE
	 *   - GU_SPECULAR
	 *   - GU_AMBIENT_AND_DIFFUSE
	 *   - GU_DIFFUSE_AND_SPECULAR
	 *
	 * @param light     - Light index
	 * @param component - Which component to set
	 * @param color     - Which color to use
	 **/
	// void sceGuLightColor(int light, int component, unsigned int color); // OP_ALC_n + OP_DLC_n + OP_SLC_n

	// Ambient/Diffuse/Specular Light Color (per light)
	mixin(LightArrayOperationStep3("OP_ALC_n", q{ gpu.state.lighting.lights[Index].ambientColor.rgba[]  = command.float4[]; }));
	mixin(LightArrayOperationStep3("OP_DLC_n", q{ gpu.state.lighting.lights[Index].diffuseColor.rgba[]  = command.float4[]; }));
	mixin(LightArrayOperationStep3("OP_SLC_n", q{ gpu.state.lighting.lights[Index].specularColor.rgba[] = command.float4[]; }));

	/**
	 * Set the specular power for the material
	 *
	 * @param power - Specular power
	 **/
	// void sceGuSpecular(float power); // OP_SPOW
	// Specular POWer (global)
	auto OP_SPOW() { gpu.state.lighting.specularPower = command.float1; }
}
