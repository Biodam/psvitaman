<#
.SYNOPSIS
    Checks GitHub Actions quota, repository visibility, and build eligibility.
.DESCRIPTION
    Ensures that builds will not incur unwanted billing charges.
    Verifies public repository status (free unlimited minutes) or checks billing quota if private.
#>

param(
    [switch]$FailOnCharge = $false
)

Write-Host "========================================"
Write-Host "PSVitaman GitHub Actions Quota & Billing Check"
Write-Host "========================================"

# 1. Check gh CLI availability
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    Write-Warning "GitHub CLI (gh) not found in PATH. Install gh or check GitHub dashboard."
    exit 0
}

# 2. Query Repository Visibility
try {
    $repoJson = gh repo view --json isPrivate,visibility,nameWithOwner | ConvertFrom-Json
    $repoName = $repoJson.nameWithOwner
    $isPrivate = $repoJson.isPrivate
    $visibility = $repoJson.visibility

    Write-Host "Repository: $repoName"
    Write-Host "Visibility: $visibility (isPrivate: $isPrivate)"

    if (-not $isPrivate) {
        Write-Host -ForegroundColor Green "`n[OK] Repository is PUBLIC!"
        Write-Host -ForegroundColor Green "GitHub Actions runner minutes are 100% FREE and UNLIMITED on public repos."
        Write-Host -ForegroundColor Green "No quota deductions or billing charges will occur."
        exit 0
    } else {
        Write-Host -ForegroundColor Yellow "`n[WARNING] Repository is PRIVATE."
        Write-Host -ForegroundColor Yellow "GitHub Actions runs will consume account billing minutes."
    }
} catch {
    Write-Warning "Could not retrieve repository info via gh: $_"
}

# 3. Query Billing / Quota for Private Repositories
$user = "Biodam"
try {
    $billing = gh api "/users/$user/settings/billing/actions" 2>$null | ConvertFrom-Json
    if ($billing) {
        $used = $billing.total_minutes_used
        $included = $billing.included_minutes
        $paid = $billing.total_paid_minutes_used
        Write-Host "`nBilling Quota for $($user):"
        Write-Host "  Included Free Minutes: $included"
        Write-Host "  Minutes Used:          $used"
        Write-Host "  Paid Minutes Used:     $paid"
        $remaining = $included - $used
        if ($remaining -le 0) {
            Write-Host -ForegroundColor Red "`n[ERROR] Account has EXHAUSTED included GitHub Actions minutes!"
            if ($FailOnCharge) {
                exit 1
            }
        } else {
            Write-Host -ForegroundColor Cyan "  Remaining Free Minutes: $remaining"
        }
    }
} catch {
    Write-Host -ForegroundColor DarkGray "`nTip: To enable direct billing API checks, run: gh auth refresh -s user"
}

if ($isPrivate -and $FailOnCharge) {
    Write-Host -ForegroundColor Red "`nBuild aborted: repository is private and FailOnCharge is enabled."
    exit 1
}
