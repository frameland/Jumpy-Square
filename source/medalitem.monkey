Strict
Import vsat
Import medals
Import extra

Class MedalItem Extends VRect
	
	Global font:AngelFont
	
	Method New(name:String, fileName:String)
		Super.New(0, 0, 0, 0)
		image = ImageCache.GetImage(RealPath("medals/" + fileName), Image.MidHandle)
		Self.name = name
		times = Medals.HowManyOf(name)
		color.Set(Color.NewBlue)
		AssertWithException(font, "MedalItem has no font set.")
	End
	
	Method Width:Float() Property
		Return image.Width()
	End
	
	Method Height:Float() Property
		Return image.Height() * 1.5 + font.TextHeight(name) * 0.7
	End
	
	Method Draw:Void()
		Local h:Float = image.Height()
		PushMatrix()
		Scale(0.8, 0.8)
		font.DrawText(name, 0, 0, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_TOP)
		PopMatrix()
		PushMatrix()
			ScaleAt(0, h * 1.55, 0.8, 0.8)
			font.DrawText("x "+times, 0, font.height + image.Height(), AngelFont.ALIGN_CENTER, AngelFont.ALIGN_TOP)
		PopMatrix()
		DrawImage(image, 0, font.height + image.Height()/2)
	End
	
	Method Name:String() Property
		Return name
	End
	
	Private
	Field image:Image
	Field name:String
	Field times:Int
End

