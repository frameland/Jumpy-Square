Strict
Import vsat
Import extra
Import particles


Class SupporterMedal Extends VSprite
	
	Method InitLocked:Void()
		Self.SetImage(RealPath("locked.png"))
		unlockTime = -1.0
		Init()
	End
	
	Method InitUnlocked:Void()
		Self.SetImage(RealPath("unlocked.png"))
		unlockTime = Vsat.Seconds
		Init()
	End
	
	Method UnlockTime:Float() Property
		Return unlockTime
	End
	
	Method UnlockTime:Void(setTime:Float) Property
		unlockTime = setTime
	End
	
	Method Render:Void()
		Super.Render()
		If unlockTime > 0
			glitter.position.Set(Self.position)
			glitter.Render()
		End
	End
	
	Method Update:Void(dt:Float)
		If unlockTime > 0
			glitter.Update(dt)
		End
	End
	
	Private
	Field unlockTime:Float = -1.0
	Field glitter:ParticleEmitter
	
	Method Init:Void()
		MidHandleImage(image)
		
		glitter = New ParticleEmitter
		glitter.InitWithSize(30)
		glitter.additiveBlend = True
		glitter.emissionRate = 20
		glitter.particleLifeSpan = 1.2
		glitter.particleLifeSpanVariance = 0.3
		glitter.positionVariance.Set(Width*0.6, Height*0.6)
		glitter.startColor.Alpha = 0.3
		glitter.endColor.Alpha = 0.8
		glitter.size.Set(3, 3)
		glitter.endSize.Set(0, 0)
		glitter.speedVariance = 5
		glitter.emissionAngleVariance = 180
		glitter.Start()
	End
	
End
