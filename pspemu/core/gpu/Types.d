module pspemu.core.gpu.Types;

import std.string;
import std.bitmanip;

struct ScreenBuffer {
	uint _address = 0;
	uint width = 512;
	uint format = 3;
	uint address(uint _address) { return this._address = (0x04_000000 | _address); }
	uint address() { return this._address; }
	uint pixelSize() {
		switch (format) {
			case 0, 1, 2: return 2;
			case 3: return 4;
			default: throw(new Exception(std.string.format("Invalid ScreenBuffer.format %d", format)));
		}
	}
	ubyte[] row(void* ptr, int row) {
		int rowsize = width * pixelSize;
		return ((cast(ubyte *)ptr) + rowsize * row)[0..rowsize];
	}
}

struct Colorf {
	union {
		struct { float[4] rgba = [0.0, 0.0, 0.0, 1.0]; }
		struct { float[3] rgb; }
		struct { float r, g, b, a; }
		struct { float red, green, blue, alpha; }
	}
	float* ptr() { return rgba.ptr; }
	static assert(this.sizeof == float.sizeof * 4);
}

struct Matrix {
	union {
		struct { float[4 * 4] cells; }
		struct { float[4][4]  rows; }
	}
	float* pointer() { return cells.ptr; }
	enum WriteMode { M4x4, M4x3 }
	const indexesM4x4 = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15];
	const indexesM4x3 = [0, 1, 2,  4, 5, 6,  8, 9, 10,  12, 13, 14];
	uint index;
	WriteMode mode;
	void reset(WriteMode mode = WriteMode.M4x4) {
		index = 0;
		this.mode = mode;
		if (mode == WriteMode.M4x3) {
			cells[11] = cells[7] = cells[3] = 0.0;
			cells[15] = 1.0;
		}
	}
	void next() { index++; index &= 0xF; }
	void write(float cell) {
		auto indexes = (mode == WriteMode.M4x4) ? indexesM4x4 : indexesM4x3;
		cells[indexes[index++ % indexes.length]] = cell;
	}
	//static assert(this.sizeof == float.sizeof * 16 + uint.sizeof);
	string toString() {
		return std.string.format(
			"(%f, %f, %f, %f)\n"
			"(%f, %f, %f, %f)\n"
			"(%f, %f, %f, %f)\n"
			"(%f, %f, %f, %f)",
			cells[0], cells[1], cells[2], cells[3],
			cells[4], cells[5], cells[6], cells[7],
			cells[8], cells[9], cells[10], cells[11],
			cells[12], cells[13], cells[14], cells[15]
		);
	}
}

struct VertexType {
	union {
		uint v;
		struct {
			mixin(bitfields!(
				uint, "texture",  2,
				uint, "color",    3,
				uint, "normal",   2,
				uint, "position", 2,
				uint, "weight",   2,
				uint, "index",    2,
				uint, "__0",      1,
				uint, "skinningWeightCount", 3,
				uint, "__1",      1,
				uint, "morphingVertexCount",   2,
				uint, "__2",      3,
				uint, "transform2D",           1,
				uint, "__3",      8
			));
		}
	}
}

static struct GpuState {
	ScreenBuffer drawBuffer;
	uint baseAddress;
	uint vertexAddress;
	uint indexAddress;
	int  clearFlags;
	VertexType vertexType;
	Colorf ambientModelColor, diffuseModelColor, specularModelColor;
	Colorf materialColor;
	Matrix projectionMatrix, worldMatrix, viewMatrix;
}

interface GpuImpl {
	void setState(GpuState *state);
	void init();
	void clear();
	void draw(VertexState[] vertexList, PrimitiveType type, PrimitiveFlags flags);
	void flush();
	void frameLoad (void* buffer);
	void frameStore(void* buffer);
}

abstract class GpuImplAbstract : GpuImpl {
	GpuState *state;
	void setState(GpuState *state) { this.state = state; }
}

// GU Primitive Types.
enum PrimitiveType { GU_POINTS = 0, GU_LINES = 1, GU_LINE_STRIP = 2, GU_TRIANGLES = 3, GU_TRIANGLE_STRIP = 4, GU_TRIANGLE_FAN = 5, GU_SPRITES = 6 };
struct PrimitiveFlags {
	bool hasWeights;
	bool hasTexture;
	bool hasColor;
	bool hasNormal;
	bool hasPosition;
	int  numWeights;
}

struct VertexState {
	float u, v;        // Texture coordinates.
	float r, g, b, a;  // Color components.
	float nx, ny, nz;  // Normal vector.
	float px, py, pz;  // Position vector.
	float weights[8];  // Weights for skinning and morphing.
}