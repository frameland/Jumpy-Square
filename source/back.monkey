Strict
Import vsat
Import extra

Class BackButton Extends VLabel
	
	Field isDown:Bool
	Field downColor:Color = Color.Orange
	
	Method New()
		Super.New("Back")
		color.Set(Color.White)
		alignVertical = True
		position.Set(Vsat.ScreenWidth * 0.05, Vsat.ScreenWidth * 0.08)
	End
	
	Method Draw:Void()
		If isDown
			downColor.UseWithoutAlpha()
		End
		SetAlpha(color.Alpha * globalAlpha.Alpha)
		
		Local length:Float = Vsat.ScreenWidth * 0.03
		PushMatrix()
		Translate(length * 1.1, -6)
		Super.Draw()
		PopMatrix()
		DrawLine(0, 0, length * 0.75, -length)
		DrawLine(0, 0, length * 0.75, length)
	End
	
	Method WasTouched:Bool(cursor:Vec2)
		Local cursorSize:Float = Vsat.ScreenWidth * 0.05
		Local length:Float = Vsat.ScreenWidth * 0.03
		Local touchsizeBufferX:Float = (size.x + length) * 0.2
		Local touchsizeBufferY:Float = size.y * 0.2
		Return RectsOverlap(cursor.x-cursorSize/2, cursor.y-cursorSize/2, cursorSize, cursorSize, position.x, position.y - size.y/2 - touchsizeBufferY, size.x+touchsizeBufferX*2, size.y+touchsizeBufferY*2)
	End
   	    
End
