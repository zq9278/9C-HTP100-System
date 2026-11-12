/* EP28062S_287 vendor active FW sequence, 2026-11-11. */
HAL_Delay(120);   

  // FW         
	HAL_Delay(120);   

	LCD_WriteCommand(0xFF);
	LCD_WriteData(0x77);
	LCD_WriteData(0x01);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteData(0x13);

	LCD_WriteCommand(0xEF);
	LCD_WriteData(0x08);

	LCD_WriteCommand(0xff);
	LCD_WriteData(0x77);
	LCD_WriteData(0x01);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteData(0x10);

	LCD_WriteCommand(0xC0);
	LCD_WriteData(0x4F);
	LCD_WriteData(0x00);

	LCD_WriteCommand(0xC1);
	LCD_WriteData(0x0B);
	LCD_WriteData(0x02);

	LCD_WriteCommand(0xC2);
	LCD_WriteData(0x10);
	LCD_WriteData(0x06);

	LCD_WriteCommand(0xCC);
	LCD_WriteData(0x18);

	LCD_WriteCommand(0xB0);
	LCD_WriteData(0xC0);
	LCD_WriteData(0x00);
	LCD_WriteData(0x47);
	LCD_WriteData(0x06);
	LCD_WriteData(0x8C);
	LCD_WriteData(0x05);
	LCD_WriteData(0x00);
	LCD_WriteData(0x04);
	LCD_WriteData(0x06);
	LCD_WriteData(0x1E);
	LCD_WriteData(0x03);
	LCD_WriteData(0x12);
	LCD_WriteData(0x0F);
	LCD_WriteData(0xAA);
	LCD_WriteData(0xB0);
	LCD_WriteData(0x7F);

	LCD_WriteCommand(0xB1);
	LCD_WriteData(0xC0);
	LCD_WriteData(0x13);
	LCD_WriteData(0x5A);
	LCD_WriteData(0x15);
	LCD_WriteData(0x17);
	LCD_WriteData(0x09);
	LCD_WriteData(0x06);
	LCD_WriteData(0x0B);
	LCD_WriteData(0x09);
	LCD_WriteData(0x23);
	LCD_WriteData(0x08);
	LCD_WriteData(0x16);
	LCD_WriteData(0x14);
	LCD_WriteData(0x6B);
	LCD_WriteData(0xB1);
	LCD_WriteData(0x7F);

	LCD_WriteCommand(0xFF);
	LCD_WriteData(0x77);
	LCD_WriteData(0x01);
	LCD_WriteData(0x00);
	LCD_WriteData(0x00);
	LCD_WriteData(0x11);

	LCD_WriteCommand(0xb0);
	LCD_WriteData(0x4D);   //Vop=4.5V

	LCD_WriteCommand(0xb1);
	LCD_WriteData(0x6B);

	LCD_WriteCommand(0xb2);
	LCD_WriteData(0x85);   //VGH=14V

	LCD_WriteCommand(0xb3);
	LCD_WriteData(0x80);

	LCD_WriteCommand(0xb5);
	LCD_WriteData(0x45);    //VGL=-9V

	LCD_WriteCommand(0xB8);
	LCD_WriteData(0x21);

	LCD_WriteCommand(0xB9);
	LCD_WriteData(0x10);

	LCD_WriteCommand(0xC0);
	LCD_WriteData(0x09);

	LCD_WriteCommand(0xC1);
	LCD_WriteData(0x78);

	LCD_WriteCommand(0xC2);
	LCD_WriteData(0x78);

	LCD_WriteCommand(0xD0);
	LCD_WriteData(0x88);

	LCD_WriteCommand(0xE0);		
	LCD_WriteData(0x00);		
	LCD_WriteData(0x3A);    
	LCD_WriteData(0x02);	
		
	LCD_WriteCommand(0xE1);		
	LCD_WriteData(0x06);		
	LCD_WriteData(0x9C);    
	LCD_WriteData(0x00);		
	LCD_WriteData(0x9C);    
	LCD_WriteData(0x05);		
	LCD_WriteData(0x9C);    
	LCD_WriteData(0x00);		
	LCD_WriteData(0x9C);    
	LCD_WriteData(0x00);		
	LCD_WriteData(0x40);		
	LCD_WriteData(0x40);	
		
	LCD_WriteCommand(0xE2);		
	LCD_WriteData(0x20);		
	LCD_WriteData(0x20);		
	LCD_WriteData(0x40);		
	LCD_WriteData(0x40);		
	LCD_WriteData(0x8C);		
	LCD_WriteData(0x8C);		
	LCD_WriteData(0x00);		
	LCD_WriteData(0x8C);		
	LCD_WriteData(0x8B);		
	LCD_WriteData(0x8C);		
	LCD_WriteData(0x00);		
	LCD_WriteData(0x8C);		
	LCD_WriteData(0x00);		
			
		
	LCD_WriteCommand(0xE3);		
	LCD_WriteData(0x00);		
	LCD_WriteData(0x00);		
	LCD_WriteData(0x22);		
	LCD_WriteData(0x22);	
		
	LCD_WriteCommand(0xE4);		
	LCD_WriteData(0x44);		
	LCD_WriteData(0x44);	
		
	LCD_WriteCommand(0xE5);		
	LCD_WriteData(0x08);		
	LCD_WriteData(0x88);		
	LCD_WriteData(0xE6);		
	LCD_WriteData(0x8C);		
	LCD_WriteData(0x0A);		
	LCD_WriteData(0x8A);		
	LCD_WriteData(0xE6);		
	LCD_WriteData(0x8C);		
	LCD_WriteData(0x0C);		
	LCD_WriteData(0x8C);		
	LCD_WriteData(0xE6);		
	LCD_WriteData(0x8C);		
	LCD_WriteData(0x0E);		
	LCD_WriteData(0x8E);		
	LCD_WriteData(0xE6);		
	LCD_WriteData(0x8C);		
		
		
		
			
	LCD_WriteCommand(0xE6);		
	LCD_WriteData(0x00);		
	LCD_WriteData(0x00);		
	LCD_WriteData(0x22);		
	LCD_WriteData(0x22);	
		
	LCD_WriteCommand(0xE7);		
	LCD_WriteData(0x44);		
	LCD_WriteData(0x44);	
		
	LCD_WriteCommand(0xE8);		
	LCD_WriteData(0x07);		
	LCD_WriteData(0x87);		
	LCD_WriteData(0xE6);		
	LCD_WriteData(0x8C);		
	LCD_WriteData(0x09);		
	LCD_WriteData(0x89);		
	LCD_WriteData(0xE6);		
	LCD_WriteData(0x8C);		
	LCD_WriteData(0x0B);		
	LCD_WriteData(0x8B);		
	LCD_WriteData(0xE6);		
	LCD_WriteData(0x8C);		
	LCD_WriteData(0x0D);		
	LCD_WriteData(0x8D);		
	LCD_WriteData(0xE6);		
	LCD_WriteData(0x8C);		

		
			

		
	LCD_WriteCommand(0xEB);		
	LCD_WriteData(0x00);		
	LCD_WriteData(0x00);		
	LCD_WriteData(0x4E);		
	LCD_WriteData(0x4E);		
	LCD_WriteData(0x00);		
	LCD_WriteData(0x00);		
	LCD_WriteData(0x00);	
		
	LCD_WriteCommand(0xEC);		
	LCD_WriteData(0x02);    
	LCD_WriteData(0x01);  
		
	LCD_WriteCommand(0xED);		
	LCD_WriteData(0xFF);		
	LCD_WriteData(0x07);		
	LCD_WriteData(0x65);		
	LCD_WriteData(0x4A);		
	LCD_WriteData(0xB2);		
	LCD_WriteData(0xB8);		
	LCD_WriteData(0x9F);		
	LCD_WriteData(0xFF);		
	LCD_WriteData(0xFF);		
	LCD_WriteData(0xF9);		
	LCD_WriteData(0x8B);		
	LCD_WriteData(0x2B);		
	LCD_WriteData(0xA4);		
	LCD_WriteData(0x56);		
	LCD_WriteData(0x70);		
	LCD_WriteData(0xFF);	
		
	LCD_WriteCommand(0xEF);
	LCD_WriteData(0x08);
	LCD_WriteData(0x08);
	LCD_WriteData(0x08);
	LCD_WriteData(0x45);
	LCD_WriteData(0x3F);
	LCD_WriteData(0x54);

	LCD_WriteCommand(0x35);
	LCD_WriteData(0x00);

  LCD_WriteCommand(0x36);
	LCD_WriteData(0x08);
	
	LCD_WriteCommand(0x11);
	HAL_Delay(120);

	LCD_WriteCommand(0x29);

	HAL_Delay(20);
