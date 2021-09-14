//--------------------------------------------------------------
// File     : stm32_ub_vga_320x240.c
// CPU      : STM32F4
// IDE      : CooCox CoIDE 1.7.0
// Module   : GPIO, TIM, MISC, DMA
// Function : VGA out by GPIO (320x240 Pixel, 8bit color)
//
// signals  : PB11      = HSync-Signal  13
//            PB12      = VSync-Signal  14
//            PE8+PE9   = color Blue    3
//            PE10-PE12 = color Green   2
//            PE13-PE15 = color red     1
//
// uses     : TIM1, TIM2
//            DMA2, Stream5
//--------------------------------------------------------------


//--------------------------------------------------------------
// Includes
//--------------------------------------------------------------
#include "stm32_ub_vga_screen.h"
#include "stdint.h"
#include "stdlib.h"


VGA_t VGA;

uint16_t UB_VGA_size_X;
uint16_t UB_VGA_size_Y;

uint16_t UB_VGA_Get_X(void)
{
  return UB_VGA_size_X;
}

uint16_t UB_VGA_Get_Y(void)
{
  return UB_VGA_size_Y;
}

void UB_VGA_Set_X(uint16_t x)
{
  UB_VGA_size_X = x;
}

void UB_VGA_Set_Y(uint16_t y)
{
  UB_VGA_size_Y = y;
}

//--------------------------------------------------------------
// Display RAM
//--------------------------------------------------------------
// uint8_t *VGA_RAM1;
// uint8_t *VGA_RAM1 __attribute__((section(".ARM.__at_0x30000000")));
// __attribute__((__section__(".halfram")))  uint8_t *VGA_RAM1;
// __attribute__((__section__(".sram01"))) uint8_t *VGA_RAM1;

uint8_t VGA_RAM1[VGA_DISPLAY_X*VGA_DISPLAY_Y]; __attribute__((section(".ARM.__at_0XC0000000")));

// extern TIM_HandleTypeDef htim1;
// extern TIM_HandleTypeDef htim2;
// extern DMA_HandleTypeDef hdma_tim1_up;

void hw_tim_start( TIM_TypeDef *tim )
{
  tim->CNT=0;
  tim->BDTR |= TIM_BDTR_MOE;
  tim->CR1  |= TIM_CR1_CEN;
}

void hw_pwm_start( TIM_TypeDef *tim )
{
  tim->CNT=0;
  tim->BDTR |= TIM_BDTR_MOE;
  tim->CR1  |= TIM_CR1_CEN;
}

void hw_pwm_chan_start( TIM_TypeDef *tim, uint32_t channel )
{
  tim->CCER |= (uint32_t)0x01 << ( (channel - 1) * 4 );
}

void hw_pwm_chan_stop( TIM_TypeDef *tim, uint32_t channel )
{
  tim->CCER &= ~((uint32_t)0x01 << ( (channel - 1) * 4 ));
}

void UB_VGA_ll_Init(void)
{

}

void UB_VGA_DMA_Init(void)
{
  /* Enable Instruction cache */
  /* Enable I-Cache */
  SCB_EnableICache();
  /* Enable D-Cache */
  // SCB_EnableDCache();
  SCB_DisableDCache();

  /* Disable the double buffer mode. */
  DMA2_Stream5->CR &= (uint32_t)(~DMA_SxCR_DBM);

  /* Configure DMA Stream data length */
  DMA2_Stream5->NDTR = UB_VGA_Get_X()/2; // LL_DMA_SetDataLength(DMA2, 5, DataLength);
  /* Configure DMA Stream source address */
  DMA2_Stream5->M0AR = VGA_RAM1;
  /* Configure DMA Stream destination address */
  DMA2_Stream5->PAR = VGA_GPIOE_ODR_ADDRESS;

  /*  Enable interrupts:
   *  Transfer error interrupt.     DMA_SxCR_TEIE
   *  Transfer complete interrupt.  DMA_SxCR_TCIE
   *  Direct mode error interrupt.  DMA_SxCR_DMEIE  */
  DMA2_Stream5->CR    |= DMA_SxCR_TCIE | DMA_SxCR_TEIE | DMA_SxCR_DMEIE;

  //-----------------------
  // Register swap and safe
  //-----------------------
  // content of CR-Register read and save
  VGA.dma2_cr_reg = DMA2_Stream5->CR;
}

//--------------------------------------------------------------
// Init VGA-Module
//--------------------------------------------------------------
void UB_VGA_Screen_Init(uint16_t vga_x, uint16_t vga_y)
{
  UB_VGA_Set_X(vga_x);
  UB_VGA_Set_Y(vga_y);

  // VGA_RAM1 = (uint8_t *)malloc(vga_x * vga_y * sizeof(uint8_t));
  // VGA_RAM1 = d1Malloc(vga_x * vga_y * sizeof(uint8_t));

  VGA.hsync_cnt = 0;
  VGA.start_adr = 0;
  VGA.dma2_cr_reg = 0;

  GPIOB->BSRR = VGA_VSYNC_Pin;

  // TIM2
  // hw_tim_start(TIM2);
  TIM2->CR1 |= TIM_CR1_CEN;  // HAL_TIM_Base_Start(&htim2);
  hw_pwm_chan_start(TIM2, 4);  // HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

  /* Enable capture/compare 3 interrupt (CC3IE).
   * LL_TIM_EnableIT_CC3(TIM_TypeDef *TIMx) */
  TIM2->DIER |= TIM_DIER_CC3IE;

  // hw_tim_start(TIM2);

  TIM1->CR1  |= TIM_CR1_CEN;   // __HAL_TIM_ENABLE(&htim1); // TIM1 Start
  
  UB_VGA_DMA_Init();

  // TIM1 update event
  TIM1->DIER |= TIM_DIER_UDE;  //__HAL_TIM_ENABLE_DMA(&htim1, TIM_DMA_UPDATE);
}

//--------------------------------------------------------------
// fill the DMA RAM buffer with one color
//--------------------------------------------------------------
void UB_VGA_FillScreen(uint8_t color)
{
  uint16_t xp,yp;

  for(yp = 0; yp < UB_VGA_Get_Y(); yp++) {
    for(xp = 0; xp < UB_VGA_Get_X(); xp++) {
      UB_VGA_SetPixel(xp, yp, color);
    }
  }
}

//--------------------------------------------------------------
// put one Pixel on the screen with one color
// Important : the last Pixel+1 from every line must be black (don't know why??)
//--------------------------------------------------------------
void UB_VGA_SetPixel(uint16_t xp, uint16_t yp, uint8_t color)
{
  if(xp >= UB_VGA_Get_X()) return;
  if(yp >= UB_VGA_Get_Y()) return;

  // Write pixel to ram
  VGA_RAM1[UB_VGA_Get_X() * yp + xp] = color;
}

uint32_t UB_VGA_GetPixel(uint16_t xp, uint16_t yp)
{
  if(xp >= UB_VGA_Get_X()) return;
  if(yp >= UB_VGA_Get_Y()) return;

  // Read pixel to ram
  return VGA_RAM1[UB_VGA_Get_X() * yp + xp];
}
