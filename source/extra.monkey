Strict
Import vsat

Const VERSION:String = "version 1.0"
Global globalAlpha:Color = New Color(Color.White)


'--------------------------------------------------------------------------
' * Functions
'--------------------------------------------------------------------------
Function VPlaySound:Void(sound:Sound, channel:Int = 0)
	#If TARGET = "html5"
		Return
	#End
	PlaySound(sound, channel)
End

Function RealPath:String(path:String)
	If IsHD()
		Return "gfxhd/" + path
	End
	Return "gfx/" + path
End

Function IsHD:Bool()
	Return Max(Vsat.ScreenWidth, Vsat.ScreenHeight) > 1200
End




'--------------------------------------------------------------------------
' * Transitions
'--------------------------------------------------------------------------
Class MoveDownTransition Extends VTransition
	
	Field startPoint:Float = 0
	
	Method New()
		Super.New()
	End
	
	Method New(duration:Float)
		Super.New(duration)
	End
	
	Method Update:Void(dt:Float)
		Super.Update(dt)
		scene.OnUpdate(dt)
	End
	
	Method Render:Void()
		PushMatrix()
		ResetMatrix()
		ResetBlend()
		Local progress:Float = Tweening(easingType, Time, 0.0, 1.0, Duration)
		Translate(0, Vsat.ScreenHeight * progress + startPoint)
		scene.OnRender()
		PopMatrix()
	End
	
	Method SetScene:Void(scene:VScene)
		Self.scene = scene
	End
	
	Method EasingType:Void(type:Int) Property
		easingType = type
	End
	
	Private
	Field scene:VScene
	Field easingType:Int = EASE_OUT_EXPO
End

Class MoveUpTransition Extends MoveDownTransition
	
	Method New()
		Super.New()
	End
	
	Method New(duration:Float)
		Super.New(duration)
	End
	
	Method Render:Void()
		PushMatrix()
		ResetMatrix()
		ResetBlend()
		Local progress:Float = Tweening(easingType, Time, 0.0, 1.0, Duration)
		Translate(0, -Vsat.ScreenHeight * progress + startPoint)
		scene.OnRender()
		PopMatrix()
	End

End





'--------------------------------------------------------------------------
' * 9 Path kinda thing
'--------------------------------------------------------------------------
Class TheGlowImage
	Global all:Image
	Global width:Int
	
	Function Init:Void()
		If all = Null
			all = LoadImage(RealPath("glow.png"))
			If IsHD()
				width = 28
			Else
				width = 14
			End
		End
	End
	
	
	Function DrawTopLeft:Void(x:Int, y:Int)
		DrawImageRect(all, x-width/2, y-width/2, 0, 0, width, width)
	End
	
	Function DrawTopRight:Void(x:Int, y:Int)
		DrawImageRect(all, x-width/2, y-width/2, all.Width()-width, 0, width, width)
	End
	
	Function DrawBottomRight:Void(x:Int, y:Int)
		DrawImageRect(all, x-width/2, y-width/2, all.Width()-width, all.Height()-width, width, width)
	End
	
	Function DrawBottomLeft:Void(x:Int, y:Int)
		DrawImageRect(all, x-width/2, y-width/2, 0, all.Height()-width, width, width)
	End
	
	
	Function DrawHorizontal:Void(x:Int, y:Int, sx:Float)
		DrawImageRect(all, x, y-width/2, width, 0, width, width, 0, sx, 1.0)
	End
	
	Function DrawVertical:Void(x:Int, y:Int, sy:Float)
		DrawImageRect(all, x-width/2, y, 0, width, width, width, 0, 1.0, sy)
	End
	
End

Function DrawGlowRect:Void(x:Float, y:Float, w:Float, h:Float)
	TheGlowImage.Init()
	
	'Corners
	TheGlowImage.DrawTopLeft(x, y)
	TheGlowImage.DrawTopRight(x+w, y)
	TheGlowImage.DrawBottomRight(x+w, y+h)
	TheGlowImage.DrawBottomLeft(x, y+h)
	
	'Horizontal Sides
	Local sx:Float = w / TheGlowImage.width - 1.0
	TheGlowImage.DrawHorizontal(x+TheGlowImage.width/2, y, sx)
	TheGlowImage.DrawHorizontal(x+TheGlowImage.width/2, y+h+1, sx)
	
	'Vertical Sides
	Local sy:Float = h / TheGlowImage.width - 1.0
	TheGlowImage.DrawVertical(x, y + TheGlowImage.width/2, sy)
	TheGlowImage.DrawVertical(x+w+1, y + TheGlowImage.width/2, sy)
	
End









