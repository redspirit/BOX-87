#pragma once

#include "PinConfig.h"
#include "Mode.h"
#include "DMAVideoBuffer.h"

class VGA {
	public:
		VGA();
		~VGA();
		bool init(const PinConfig cfgPins, const Mode mode, int bits);
		bool start();
		bool show();
		inline int width() const  { return mode.hRes; }
		inline int height() const { return mode.vRes; }		
		void clear(uint8_t color);
		void fillRect(int x, int y, int w, int h, int rgb);
		void dot(int x, int y, int rgb);
    	inline uint8_t* getLinePtr8(int y) {
			// указатель на начало строки (8 бит на пиксель)
			return dmaBuffer->getLineAddr8(y, backBuffer);
		} 
    	uint8_t* getLinePtr8Safe(int y);	// с проверкой диапазона y

	private:
		Mode mode;
		int bufferCount;
		int bits;
		PinConfig pins;
		int backBuffer;
		DMAVideoBuffer *dmaBuffer;
		bool usePsram;
		int dmaChannel;

	protected:
		void attachPinToSignal(int pin, int signal);
};