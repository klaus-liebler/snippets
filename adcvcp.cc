
#include <algorithm>
#include <cmath>
#include "adcvcp.hh"
#include "util.hh"
#include "usbutils.hh"
#include "main.hh"

/// Gibt die Register eines DMA-Channels (startet bei 0) zurück.
static inline DMA_Channel_TypeDef* getDMA (uint8_t iChannel) {
	return reinterpret_cast<DMA_Channel_TypeDef*> (DMA1_BASE + 0x08 + 20 * iChannel);
}

/// Indirekt aufgerufen durch USBPhys; setzt alles zurück.
void ADCVCP::onReset () {
	EPBuffer::onReset ();

	// Setze alle Daten zurück

	m_usb2uartBuffer.reset ();
	m_uart2usbBuffer.reset ();
	m_rxUSARTBytes = 0;
	m_txUSARTBytes = 0;
	m_txUSBBytes = 0;
	m_usbReceiving = false;
	m_usbTransmitting = false;
	confUSART ();

	setupDMA (true);

	prepareUSBreceive ();
}

/// Wird einmal von der main() aufgerufen, initialisiert die Peripherie.
void ADCVCP::init () {

}

/// Konfiguriert den angegebenen DMA-Channel zum Senden oder Empfangen.
void VCP::setupDMA (uint8_t channel, uint8_t* memBuf, uint16_t length, bool TX) {
	DMA_Channel_TypeDef* pDMA = getDMA (channel);

	// Channel abschalten
	pDMA->CCR = 0;
	// Interrupts löschen
	clearDMAInt (channel);

	// Falls keine Bytes übertragen werden, lasse den Channel aus.
	if (length) {
		// Konfiguriere Channel
		pDMA->CNDTR = length;
		pDMA->CPAR = reinterpret_cast<uint32_t> (&usart ()->DR);
		pDMA->CMAR = reinterpret_cast<uint32_t> (memBuf);
		// Starte Übertragung
		pDMA->CCR = DMA_CCR_MINC | DMA_CCR_TCIE | DMA_CCR_EN | (TX ? DMA_CCR_DIR : 0);
	}
}

/// Konfiguriert beide DMA-Channel falls sie nichts zu tun haben oder force==true ist.
void VCP::setupDMA (bool force) {
	if (force || !m_usartReceiving) {
		// Anzahl empfangbarer Bytes
		uint16_t wa = static_cast<uint16_t> (std::min<size_t> (0xFFFF, m_uart2usbBuffer.writeAvailable ()));
		m_rxUSARTBytes = wa;
		m_usartReceiving = wa;
		// Konfiguriere DMA. Falls "wa"=0 ist, wird Channel nur abgeschaltet
		setupDMA (m_iDMA_RX, m_uart2usbBuffer.write(), wa, false);
	}

	if (force || !m_usartTransmitting) {
		// Anzahl sendbarer Bytes
		uint16_t ra = static_cast<uint16_t> (std::min<size_t> (0xFFFF, m_usb2uartBuffer.readAvailable ()));
		m_txUSARTBytes = ra;
		m_usartTransmitting = ra;
		// Konfiguriere DMA. Falls "ra"=0 ist, wird Channel nur abgeschaltet
		setupDMA (m_iDMA_TX, m_usb2uartBuffer.read (), ra, true);
	}
}

/// Löscht die Interrupts des angegebenen DMA-Channel im NVIC und DMA selbst
void VCP::clearDMAInt (uint8_t channel) {
	// Interrupts löschen
	DMA1->IFCR = 0xF << (channel * 4);
	// Auch im NVIC löschen
	NVIC_ClearPendingIRQ(static_cast<IRQn_Type> (DMA1_Channel1_IRQn+channel));
}

/**
 * Sollte von der ISR aufgerufen werden, wenn der empfangende DMA-Transfer fertig ist.
 * Übernimmt die empfangenen Bytes und bereitet neuen Transfer vor.
 */
void VCP::onDMA_RX_Finish () {
	DMA_Channel_TypeDef* pDMA = getDMA (m_iDMA_RX);
	// DMA Channel abschalten
	pDMA->CCR = 0;
	// Interrupts löschen
	clearDMAInt (m_iDMA_RX);
	// Wenn vorzeitig aufgerufen, ist dmaRemaining < m_rxUSARTBytes
	uint16_t dmaRemaining = static_cast<uint16_t> (pDMA->CNDTR);
	if (dmaRemaining >= m_rxUSARTBytes) return;
	// Empfangene Bytes übernehmen
	m_uart2usbBuffer.writeFinish (m_rxUSARTBytes - dmaRemaining);
	m_usartReceiving = false;
	// Tausche Puffer, auch wenn Puffer noch nicht voll. Dadurch werden "kurze" Byte-Folgen
	// auch übernommen, aber die Effizienz auf USB-Seite sinkt.
	m_uart2usbBuffer.swap ();

	// Nächsten Transfer vorbereiten
	setupDMA ();
	prepareUSBtransmit ();
}

/**
 * Sollte von der ISR aufgerufen werden, wenn der sendende DMA-Transfer fertig ist.
 * Löscht die gesendeten Bytes und bereitet neuen Transfer vor.
 */
void VCP::onDMA_TX_Finish () {
	DMA_Channel_TypeDef* pDMA = getDMA (m_iDMA_TX);
	// DMA Channel abschalten
	pDMA->CCR = 0;
	// Wenn vorzeitig aufgerufen, ist dmaRemaining < m_txUSARTBytes
	uint16_t dmaRemaining = static_cast<uint16_t> (pDMA->CNDTR);
	if (dmaRemaining >= m_txUSARTBytes) return;

	uint16_t sent = static_cast<uint16_t> (m_txUSARTBytes - dmaRemaining);
	// Interrupts löschen
	clearDMAInt (m_iDMA_TX);
	myassert (m_usb2uartBuffer.readAvailable() >= sent);
	// Gesendete Bytes löschen (Puffer frei machen)
	m_usb2uartBuffer.readFinish (sent);
	m_usartTransmitting = false;

	// Nächsten Transfer vorbereiten
	setupDMA ();
	prepareUSBreceive ();
}

/// Wird via VCP_DataEP::onReceive aufgerufen, wenn per USB Daten für den VCP angekommen sind.
void VCP::onReceive (bool, size_t rxBytes) {
	size_t wa = m_usb2uartBuffer.writeAvailable ();
	myassert (rxBytes <= wa);

	// Frage Puffer ab
	size_t l = static_cast<uint16_t> (std::min<size_t> (wa, rxBytes));
	uint8_t* buf = m_usb2uartBuffer.write ();

	// Hole Daten aus USB-Puffer in Doppelpuffer
	getDataEP ()->getReceivedData (buf, l);
	// Schreibvorgang abschließen
	m_usb2uartBuffer.writeFinish (l);
	m_usbReceiving = false;

	// Tausche Puffer, auch wenn Puffer noch nicht voll. Dadurch werden "kurze" Byte-Folgen
	// auch übernommen, aber die Effizienz auf USB-Seite sinkt.
	m_usb2uartBuffer.swap ();
	prepareUSBreceive ();
	setupDMA ();
}

/// Wird via VCP_DataEP::onTransmit aufgerufen, wenn per USB Daten vom VCP gesendet wurden.
void VCP::onTransmit () {
	myassert (m_uart2usbBuffer.readAvailable () >= m_txUSBBytes);
	// Gebe Puffer frei.
	m_uart2usbBuffer.readFinish (m_txUSBBytes);
	m_usbTransmitting = false;

	// Nächsten Transfer vorbereiten
	prepareUSBtransmit ();
	setupDMA ();
}

/// Startet den Empfang von Daten per USB.
void VCP::prepareUSBreceive () {
	if (m_usbReceiving) return;

	size_t wa = m_usb2uartBuffer.writeAvailable ();
	// Stelle sicher, dass nicht weniger als dataEpMaxPacketSize Bytes empfangen werden;
	// sonst sendet der Host zu viele Daten die dann von der Peripherie verweigert würden.
	if (wa >= dataEpMaxPacketSize) {
		// Empfange so viel wie möglich.
		size_t count = static_cast<uint16_t> (std::min<size_t> (wa, getDataEP ()->getRxBufLength ()));
		getDataEP ()->receivePacket (count);
		m_usbReceiving = true;
	} else
		m_usbReceiving = false;
}

void VCP::prepareUSBtransmit () {
	if (m_usbTransmitting) return;
	size_t ra = m_uart2usbBuffer.readAvailable ();
	// Beim Senden dürfen auch kurze Pakete auftreten.
	if (ra > 0) {
		// Sende so viel wie möglich.
		size_t count = std::min<size_t> (ra, getDataEP ()->getTxBufLength ());
		m_txUSBBytes = count;
		m_usbTransmitting = true;
		getDataEP ()->transmitPacket (m_uart2usbBuffer.read (), count);
	} else
		m_usbTransmitting = false;
}

/**
 * Sollte von der ISR des USART aufgerufen werden. Wenn ein "IDLE"-Frame erkannt wurde, d.h.
 * eine Lücke im USART-Datenstrom, wird der DMA-Transfer vorzeitig abgebrochen und die bisher
 * empfangenen Daten direkt abgesendet. So werden auch kurze Byte-Folgen abgesendet.
 * Alternativ könnte auch ein richtiger Timeout genutzt werden.
 */
void VCP::onUSART_IRQ () {
	// Prüfe auf IDLE-Interrupt
	if (usart ()->SR & USART_SR_IDLE) {
		// SR und DR müssen gelesen werden, um Interrupt zu quittieren
		static_cast<void> (usart ()->DR);
		// Wenn überhaupt schon etwas empfangen wurde...
		if (m_usartReceiving && getDMA (m_iDMA_RX)->CNDTR < m_rxUSARTBytes) {
			// Simuliere DMA-Interrupt
			onDMA_RX_Finish ();
		}
	}
}

