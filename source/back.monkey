Strict
Import vsat
Import extra

Class BackButton Extends VLabel
	
	Field isDown:Bool
	Field downColor:Color = Color.Orange
	
	Method New()
		Super.New("Back")
		color.Set(Color.White)
		alignVertical = AngelFont.ALIGN_CENTER
		position.Set(Vsat.ScreenWidth * 0.05, Vsat.ScreenWidth * 0.06)
		image = ImageCache.GetImage(RealPath("back.png"))
		MidHandleImage(image)
	End
	
	Method Draw:Void()
		If isDown
			downColor.UseWithoutAlpha()
		End
		SetAlpha(color.Alpha * globalAlpha.Alpha)
		
		Local length:Float = Vsat.ScreenWidth * 0.03
		DrawImage(image, length, length*0.5)
		
		PushMatrix()
			If IsHD()
				Translate(image.Width()*2, 5)
			Else
				Translate(image.Width()*1.5, 5)
			End
			Super.Draw()
		PopMatrix()
	End
	
	Method WasTouched:Bool(cursor:Vec2)
		Local cursorSize:Float = Vsat.ScreenWidth * 0.05
		Local length:Float = Vsat.ScreenWidth * 0.03
		Local touchsizeBufferX:Float = (size.x + length) * 0.2
		Local touchsizeBufferY:Float = size.y * 0.2
		Return RectsOverlap(cursor.x-cursorSize/2, cursor.y-cursorSize/2, cursorSize, cursorSize, position.x, position.y - size.y/2 - touchsizeBufferY, size.x+touchsizeBufferX*2, size.y+touchsizeBufferY*2)
	End
   	    
	Private
	Field image:Image
	
End
