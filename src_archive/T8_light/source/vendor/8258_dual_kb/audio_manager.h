#ifndef AUDIO_MANAGER_H_
#define AUDIO_MANAGER_H_

/**
 * @brief  open audio
 * @note   
 * @retval None
 */
void audio_enable();

/**
 * @brief  close audio
 * @note   
 * @retval None
 */
void audio_disable();

/**
 * @brief  dispatch audio data to client
 * @note   
 * @retval None
 */
void audio_dispatch();

#endif