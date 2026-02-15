#pragma once

class PinConfig;
class PinConfig
{
	public:
		static const PinConfig VGAPurple;
	public:
		int r[3];
		int g[3];
		int b[3];
		int hSync, vSync;

	PinConfig() {};

	PinConfig(
		int r0, int r1, int r2,
		int g0, int g1, int g2, 
		int b0, int b1, int b2,
		int hSync, int vSync)
		{
			r[0] = r0; r[1] = r1; r[2] = r2; 
			g[0] = g0; g[1] = g1; g[2] = g2; 
			b[0] = b0; b[1] = b1; b[2] = b2;
			this->hSync = hSync;
			this->vSync = vSync;
		}
};