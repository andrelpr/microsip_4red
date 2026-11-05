/*
 * [fork] config_site.h do PJSIP para a compilacao do MicroSIP personalizado.
 * Copiado pelo CI para pjlib/include/pj/config_site.h antes de compilar o pjproject.
 *
 * Espelha o que o binario oficial 3.22.x usa (verificado no MicroSIP.exe oficial):
 * WMME, OpenSSL (TLS + DTLS-SRTP), Opus, G.722, G.722.1, GSM, iLBC, L16, Speex.
 * Diferencas: sem video (sem FFmpeg/SDL/VPX) e sem G.729 (IPP/bcg729).
 */

#define PJ_IS_LITTLE_ENDIAN 1
#define PJ_IS_BIG_ENDIAN 0

/* TLS e SRTP via OpenSSL (vcpkg, x86-windows-static) */
#define PJ_HAS_SSL_SOCK 1
#define PJ_SSL_SOCK_IMP PJ_SSL_SOCK_IMP_OPENSSL
#define PJMEDIA_HAS_SRTP 1
#define PJMEDIA_SRTP_HAS_DTLS 1

/* Audio */
#define PJMEDIA_AUDIO_DEV_HAS_WMME 1
#define PJMEDIA_HAS_OPUS_CODEC 1
#define PJMEDIA_HAS_G722_CODEC 1
#define PJMEDIA_HAS_G7221_CODEC 1
#define PJMEDIA_HAS_GSM_CODEC 1
#define PJMEDIA_HAS_ILBC_CODEC 1
#define PJMEDIA_HAS_L16_CODEC 1
#define PJMEDIA_HAS_SPEEX_CODEC 1

/* Sem video */
#define PJMEDIA_HAS_VIDEO 0

/* Mensagens SIP grandes (SDP com muitos codecs, Asterisk com muitos headers) */
#define PJSIP_MAX_PKT_LEN 8000
